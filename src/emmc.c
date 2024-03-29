// # Resources
//
// - <http://yannik520.github.io/sdio.html>
// - <https://www.raspberrypi.org/app/uploads/2012/02/BCM2835-ARM-Peripherals.pdf> (for RPi3 but the EMMC controller is the same except for the base address)
// - <https://web.archive.org/web/20170107083932/http://lib.store.yahoo.net/lib/yhst-92940736592539/ts4gsdhc150.pdf>
// - <https://web.archive.org/web/20160301223058/http://www.circlemud.org/jelson/sdcard/SDCardStandardv1.9.pdf>

#include "base.h"
#include "emmc.h"
#include "gpio.h"
#include "log.h"
#include "mailbox.h"
#include "sleep.h"
#include "try.h"

/*
The marshaled command has the following structure, but is a `u32` for portability and ergonomics:
struct {
  // 0..1
  u8 resp_a: 1;
  // 1..2
  u8 block_count: 1;
  // 2..4
  u8 auto_command: 2;
  // 4..5
  u8 direction: 1;
  // 5..6
  u8 multiblock: 1;
  // 6..16
  u16 resp_b: 10;
  // 16..18
  u8 response_type: 2;
  // 18..19
  u8 _res0: 1;
  // 19..20
  u8 crc_enable: 1;
  // 20..21
  u8 idx_enable: 1;
  // 21..22
  u8 is_data: 1;
  // 22..24
  u8 type: 2;
  // 24..30
  u8 index: 6;
  // 30..32
  u8 _res1: 2;
};
*/
typedef u32 emmc_marshaled_command_t;

static struct {
	u32 arg2;
	u32 block_size_count;
	u32 arg1;
	u32 command;
	u32 response[4];
	u32 data;
	u32 status;
	u32 control[2];
	// Each bit indicates the occurrence of an event. Writing 1 to a bit will clear it for subsequent reads.
	u32 interrupt_flags;
	// If a given bit is set in this "mask", that interrupt will be *un*masked.
	u32 interrupt_mask;
	// Each bit specifies whether the setting of that given "interrupt flag" bit will generate an actual hardware interrupt.
	u32 interrupt_enable;
	u32 control2;
	// There are more registers but none of them are necessary for this driver.
} volatile* const EMMC = (void volatile*)(PERIPHERAL_BASE + 0x34'0000);

enum : u32 {
	TIMEOUT_DEFAULT = 2'000,

	RESPONSE_NONE = 0b00,
	RESPONSE_136 = 0b01,
	RESPONSE_48 = 0b10,
	RESPONSE_48_BUSY = 0b11,

	SD_CLOCK_ID = 400'000,
	SD_CLOCK_NORMAL = 25'000'000,
	SD_CLOCK_HIGH = 50'000'000,
	SD_CLOCK_100 = 100'000'000,
	SD_CLOCK_208 = 208'000'000,

	CTRL0_RPI4_ENABLE_BUS_POWER = 0xf00,

	CTRL1_CLOCK_DIVIDER_MASK = 0xffe0,
	DIVISOR_MAX = (1 << 10) - 1,
	CTRL1_CLOCK_ENABLE_INTERNAL = 1 << 0,
	CTRL1_CLOCK_STABLE = 1 << 1,
	CTRL1_CLOCK_ENABLE = 1 << 2,
	CTRL1_CLOCK_GENERATOR_SELECT = 1 << 5,
	CTRL1_DATA_TIMEOUT_SHIFT = 16,
	CTRL1_DATA_TIMEOUT_MASK = 0b1111 << CTRL1_DATA_TIMEOUT_SHIFT,
	CTRL1_RESET_HOST = 1 << 24,
	CTRL1_RESET_COMMAND = 1 << 25,
	CTRL1_RESET_DATA = 1 << 26,

	CTRL1_RESET_ALL = CTRL1_RESET_DATA | CTRL1_RESET_COMMAND | CTRL1_RESET_HOST,

	STATUS_COMMAND_INHIBIT = 1 << 0,
	STATUS_DATA_INHIBIT = 1 << 1,

	CARD_STATUS_READY_FOR_DATA = 1 << 8,
	CARD_STATUS_CURRENT_STATE_SHIFT = 9,
	CARD_STATUS_CURRENT_STATE_MASK = 0xf,
	CARD_STATUS_CURRENT_STATE_STANDBY = 3,
	CARD_STATUS_CURRENT_STATE_TRANSFER = 4,

	INTERRUPT_COMMAND_DONE = 1 << 0,
	INTERRUPT_DATA_DONE = 1 << 1,
	INTERRUPT_WRITE_READY = 1 << 4,
	INTERRUPT_READ_READY = 1 << 5,
	INTERRUPT_DATA_ERROR = 1 << 15,
	INTERRUPT_COMMAND_TIMEOUT = 1 << 16,
	INTERRUPT_DATA_TIMEOUT = 1 << 20,
	INTERRUPT_ERROR_MASK = 0xffff'0000,

	OCR_VOLTAGE_WINDOW = 0x00ff'8000,
	OCR_SDHC_SUPPORT = 1 << 30,
	OCR_DONE = 1u << 31,

	RCA_MASK = 0xffff'0000,

	CONDITION_VOLTAGE_SHIFT = 8,
	CONDITION_3_3V = 1,
	// A dummy byte that is simply echoed back by the card.
	// This alternating pattern of 0s and 1s is recommended.
	CONDITION_ECHO = 0b1010'1010,
	CONDITION = CONDITION_3_3V << CONDITION_VOLTAGE_SHIFT | CONDITION_ECHO,
	CONDITION_MASK = 0xfff,

	CMD_BLOCK_COUNTER = 1 << 1,
	// Unset = host to card.
	CMD_CARD_TO_HOST = 1 << 4,
	CMD_MULTIBLOCK = 1 << 5,
	CMD_IS_DATA = 1 << 21,
	CMD_AUTO_12 = 0b01 << 2,

	CMD_SHIFT_RESPONSE_TYPE = 16,
	CMD_SHIFT_CRC = 19,
	CMD_SHIFT_INDEX = 24,

	CMD_MASK_INDEX = (1 << 6) - 1,
	CMD_MASK_RESPONSE_TYPE = (1 << 2) - 1,
};

#define MAKE_EMMC_COMMAND(_response_type, _crc_enable, _index) \
	(((_response_type) << CMD_SHIFT_RESPONSE_TYPE) | ((_crc_enable) << CMD_SHIFT_CRC) | ((_index) << CMD_SHIFT_INDEX))

enum emmc_command : u32 {
	command_go_idle = 0,
	command_send_card_id = MAKE_EMMC_COMMAND(RESPONSE_136, 1, 2),
	command_send_relative_address = MAKE_EMMC_COMMAND(RESPONSE_48, 1, 3),
	command_io_set_operating_conditions = MAKE_EMMC_COMMAND(RESPONSE_136, 0, 5),
	command_select_card = MAKE_EMMC_COMMAND(RESPONSE_48_BUSY, 1, 7),
	command_send_if_condition = MAKE_EMMC_COMMAND(RESPONSE_48, 1, 8),
	command_get_card_specific_data = MAKE_EMMC_COMMAND(RESPONSE_136, 1, 9),
	command_set_block_length = MAKE_EMMC_COMMAND(RESPONSE_48, 1, 16),
	command_read_block = MAKE_EMMC_COMMAND(RESPONSE_48, 1, 17) | CMD_CARD_TO_HOST | CMD_IS_DATA,
	command_read_multiple = MAKE_EMMC_COMMAND(RESPONSE_48, 1, 18) | CMD_BLOCK_COUNTER | CMD_AUTO_12 | CMD_CARD_TO_HOST | CMD_MULTIBLOCK | CMD_IS_DATA,
	command_write_block = MAKE_EMMC_COMMAND(RESPONSE_48, 1, 24) | CMD_IS_DATA,
	command_write_multiple = MAKE_EMMC_COMMAND(RESPONSE_48, 1, 25) | CMD_BLOCK_COUNTER | CMD_AUTO_12 | CMD_MULTIBLOCK | CMD_IS_DATA,
	command_check_ocr = MAKE_EMMC_COMMAND(RESPONSE_48, 0, 41),
	command_send_scr = MAKE_EMMC_COMMAND(RESPONSE_48, 1, 51) | CMD_CARD_TO_HOST | CMD_IS_DATA,
	command_application_prefix = MAKE_EMMC_COMMAND(RESPONSE_48, 1, 55),
};

static struct {
	union read_or_write {
		u32* read;
		u32 const* write;
	} buffer;
	u32 transfer_blocks;
	u32 last_response[4];
	u32 relative_card_address;
	u32 base_clock;
	u32 last_error;
	// In blocks.
	u32 capacity;
	// High-capacity cards use block-based addressing, while normal-capacity cards use byte-based addressing, so we need to keep track of what kind our card is.
	bool high_capacity;
	u8 _pad0[3];
} device = { 0 };

static bool wait_reg_mask(u32 volatile* const reg, u32 const mask, bool const wanted_value, u32 const timeout_millis) {
	u32 const timeout_micros = timeout_millis * 1'000;
	for (u32 cycles = 0; cycles < timeout_micros; ++cycles) {
		if ((bool)(*reg & mask) == wanted_value) {
			return true;
		}

		sleep_micros(1);
	}

	return false;
}

static u32 get_clock_divider(u32 const base_clock, u32 const target_rate) {
	assert(base_clock % target_rate == 0, "base clock not a multiple of target rate");
	u32 const divisor = base_clock / target_rate;
	assert(divisor <= DIVISOR_MAX, "divisor is too large, will not fit");
	u32 const lsb8 = divisor & 0xff;
	u32 const msb2 = (divisor >> 8) & 0b11;
	return (lsb8 << 8) | (msb2 << 6);
}

static bool switch_clock_rate(u32 const base_clock, u32 const target_rate) {
	LOG_TRACE("switching clock rate to %u with base clock %u", target_rate, base_clock);

	// Get divider here so we fail early if we cannot find one.
	u32 const divider = get_clock_divider(base_clock, target_rate);

	wait_reg_mask(&EMMC->status, STATUS_COMMAND_INHIBIT | STATUS_DATA_INHIBIT, false, 1'000);

	u32 c1 = EMMC->control[1];

	c1 &= ~CTRL1_CLOCK_ENABLE;
	EMMC->control[1] = c1;

	c1 &= ~CTRL1_CLOCK_DIVIDER_MASK;
	c1 |= divider;
	EMMC->control[1] = c1;

	c1 |= CTRL1_CLOCK_ENABLE;
	EMMC->control[1] = c1;

	LOG_TRACE("switched clock rate to %u", target_rate);

	return true;
}

static bool emmc_setup_clock(void) {
	LOG_DEBUG("setting up clock");

	EMMC->control2 = 0;

	TRY_MSG(mailbox_get_clock_rate(mailbox_clock_emmc, &device.base_clock))

	u32 c1 = EMMC->control[1];

	c1 |= CTRL1_CLOCK_ENABLE_INTERNAL;

	c1 &= ~CTRL1_CLOCK_DIVIDER_MASK;
	c1 |= get_clock_divider(device.base_clock, SD_CLOCK_ID);

	c1 &= ~CTRL1_DATA_TIMEOUT_MASK;
	c1 |= 11u << CTRL1_DATA_TIMEOUT_SHIFT;

	EMMC->control[1] = c1;

	TRY_MSG(wait_reg_mask(&EMMC->control[1], CTRL1_CLOCK_STABLE, true, TIMEOUT_DEFAULT))

	EMMC->control[1] |= CTRL1_CLOCK_ENABLE;

	LOG_DEBUG("clock is set up");

	return true;
}

static void set_last_error(u32 const interrupt_flags) {
	device.last_error = interrupt_flags & INTERRUPT_ERROR_MASK;
}

static bool do_data_transfer(emmc_marshaled_command_t const command) {
	LOG_TRACE("doing data transfer for command %x", command);

	u32 rw_interrupt_flag;
	bool write;

	if (command & CMD_CARD_TO_HOST) {
		rw_interrupt_flag = INTERRUPT_READ_READY;
		write = false;
	} else {
		rw_interrupt_flag = INTERRUPT_WRITE_READY;
		write = true;
	}

	for (u32 block = 0; block < device.transfer_blocks; ++block) {
		wait_reg_mask(&EMMC->interrupt_flags, rw_interrupt_flag | INTERRUPT_DATA_ERROR, true, TIMEOUT_DEFAULT);

		u32 const interrupt_flags = EMMC->interrupt_flags & (INTERRUPT_ERROR_MASK | rw_interrupt_flag);
		EMMC->interrupt_flags = rw_interrupt_flag | INTERRUPT_DATA_ERROR;

		if (interrupt_flags != rw_interrupt_flag) {
			set_last_error(interrupt_flags);
			return false;
		}

		// I would prefer to put the branches inside the loop, but LICM doesn't seem to be able to optimize it.
		// This collapses 128/256 branches to 1 in an incredibly hot loop.
		if (write) {
			if (device.buffer.write != NULL) {
				for (u32 i = 0; i < EMMC_BLOCK_SIZE; i += sizeof(EMMC->data)) {
					EMMC->data = *device.buffer.write++;
				}
			} else {
				for (u32 i = 0; i < EMMC_BLOCK_SIZE; i += sizeof(EMMC->data)) {
					EMMC->data = 0;
				}
			}
		} else {
			for (u32 i = 0; i < EMMC_BLOCK_SIZE; i += sizeof(EMMC->data)) {
				*device.buffer.read++ = EMMC->data;
			}
		}
	}

	return true;
}

static bool emmc_command(emmc_marshaled_command_t const command, u32 const arg, u32 const timeout) {
	LOG_DEBUG("sending EMMC command %x with arg %u and timeout %u", command, arg, timeout);

	TRY_MSG(device.transfer_blocks < (1 << 16))

	device.last_error = 0;

	EMMC->block_size_count = EMMC_BLOCK_SIZE | (device.transfer_blocks << 16);
	EMMC->arg1 = arg;
	EMMC->command = command;

	LOG_TRACE("waiting for \"command done\" interrupt flag");

	TRY_MSG(wait_reg_mask(&EMMC->interrupt_flags, INTERRUPT_DATA_ERROR | INTERRUPT_COMMAND_DONE, true, timeout))

	u32 interrupt_flags = EMMC->interrupt_flags;

	EMMC->interrupt_flags = INTERRUPT_ERROR_MASK | INTERRUPT_COMMAND_DONE;

	if ((interrupt_flags & (INTERRUPT_ERROR_MASK | INTERRUPT_COMMAND_DONE)) != INTERRUPT_COMMAND_DONE) {
		set_last_error(interrupt_flags);
		return false;
	}

	LOG_TRACE("copying response");

	u32 const response_type = (command >> CMD_SHIFT_RESPONSE_TYPE) & CMD_MASK_RESPONSE_TYPE;
	switch (response_type) {
		case RESPONSE_48:
		case RESPONSE_48_BUSY:
			device.last_response[0] = EMMC->response[0];
			break;

		case RESPONSE_136:
			device.last_response[0] = EMMC->response[0];
			device.last_response[1] = EMMC->response[1];
			device.last_response[2] = EMMC->response[2];
			device.last_response[3] = EMMC->response[3];
			break;
	}

	bool const is_data = command & CMD_IS_DATA;
	if (is_data) {
		do_data_transfer(command);
	}

	if (response_type == RESPONSE_48_BUSY || is_data) {
		LOG_TRACE("waiting for \"data done\" interrupt flag");

		wait_reg_mask(&EMMC->interrupt_flags, INTERRUPT_DATA_ERROR | INTERRUPT_DATA_DONE, true, TIMEOUT_DEFAULT);
		interrupt_flags = EMMC->interrupt_flags & (INTERRUPT_ERROR_MASK | INTERRUPT_DATA_DONE);

		EMMC->interrupt_flags = INTERRUPT_ERROR_MASK | INTERRUPT_DATA_DONE;

		if ((interrupt_flags & ~INTERRUPT_DATA_TIMEOUT) != INTERRUPT_DATA_DONE) {
			set_last_error(interrupt_flags);
			return false;
		}

		EMMC->interrupt_flags = INTERRUPT_ERROR_MASK | INTERRUPT_DATA_DONE;
	}

	return true;
}

static bool reset_command(void) {
	LOG_DEBUG("sending reset command");

	EMMC->control[1] |= CTRL1_RESET_COMMAND;

	return wait_reg_mask(&EMMC->control[1], CTRL1_RESET_COMMAND, false, 10'000);
}

static bool emmc_app_command(enum emmc_command const command, u32 const arg, u32 const timeout) {
	return emmc_command(command_application_prefix, device.relative_card_address, TIMEOUT_DEFAULT) && emmc_command(command, arg, timeout);
}

static bool check_v2_card(void) {
	LOG_DEBUG("checking v2 card");

	if (!emmc_command(command_send_if_condition, CONDITION, 200)) {
		if (device.last_error & INTERRUPT_COMMAND_TIMEOUT) {
			TRY(reset_command())
			EMMC->interrupt_flags = INTERRUPT_COMMAND_TIMEOUT;
		}
		return false;
	}

	return (device.last_response[0] & CONDITION_MASK) == CONDITION;
}

static bool check_usable_card(void) {
	LOG_DEBUG("checking for usable card");

	if (emmc_command(command_io_set_operating_conditions, 0, 1'000)) {
		return true;
	} else {
		if (device.last_error == 0) {
			return true;
		} else if (device.last_error & INTERRUPT_COMMAND_TIMEOUT) {
			// Probably not an SDIO card, but not important, and the card can still be considered usable.
			TRY(reset_command())
			EMMC->interrupt_flags = INTERRUPT_COMMAND_TIMEOUT;
			return true;
		} else {
			return false;
		}
	}
}

static bool check_sdhc_support(bool const v2_card) {
	LOG_DEBUG("checking for SDHC support");

	while (true) {
		u32 const v2_flags = v2_card ? OCR_SDHC_SUPPORT : 0;

		TRY_MSG(emmc_app_command(command_check_ocr, OCR_VOLTAGE_WINDOW | v2_flags, TIMEOUT_DEFAULT))

		if (device.last_response[0] & OCR_DONE) {
			device.high_capacity = device.last_response[0] & OCR_SDHC_SUPPORT;
			return true;
		}

		sleep_micros(500'000);
	}
}

static bool check_ocr(void) {
	LOG_DEBUG("checking OCR");

	return emmc_app_command(command_check_ocr, 0, TIMEOUT_DEFAULT);
}

static bool check_rca(void) {
	LOG_DEBUG("checking RCA");

	// We need to send this command before the "send relative address" command even though we don't care about the card ID.
	TRY_MSG(emmc_command(command_send_card_id, 0, TIMEOUT_DEFAULT))

	TRY_MSG(emmc_command(command_send_relative_address, 0, TIMEOUT_DEFAULT))

	device.relative_card_address = device.last_response[0] & RCA_MASK;

	TRY_MSG(device.last_response[0] & CARD_STATUS_READY_FOR_DATA)

	return true;
}

static bool select_card(void) {
	LOG_DEBUG("selecting card");

	TRY_MSG(emmc_command(command_select_card, device.relative_card_address, TIMEOUT_DEFAULT))

	u32 const status = (device.last_response[0] >> CARD_STATUS_CURRENT_STATE_SHIFT) & CARD_STATUS_CURRENT_STATE_MASK;

	if (status != CARD_STATUS_CURRENT_STATE_STANDBY && status != CARD_STATUS_CURRENT_STATE_TRANSFER) {
		LOG_ERROR("invalid current-state %u in response to RCA command", status);
		return false;
	}

	return true;
}

static void enable_bus_power(void) {
	LOG_DEBUG("enabling bus power");

	EMMC->control[0] |= CTRL0_RPI4_ENABLE_BUS_POWER;
}

static bool check_card_specific_data(void) {
	TRY_MSG(emmc_command(command_get_card_specific_data, device.relative_card_address, TIMEOUT_DEFAULT))

	if (!device.high_capacity) {
		LOG_ERROR("standard-capacity cards not yet implemented");
		return false;
	}

	// Bits 48..70 of the whole response.
	// The controller cuts off the 7-bit CRC + 1 bit so we subtract 8 from that offset.
	u32 const capacity_raw = (device.last_response[1] >> 8) & ((1 << 22) - 1);
	// Actual capacity in bytes = (raw capacity + 1) * 512KiB
	// This implementation must be adjusted if the block size changes.
	_Static_assert(EMMC_BLOCK_SIZE == 512, "The implementation must be adjusted to account for the new block size.");
	device.capacity = (capacity_raw + 1) * 1024;

	return true;
}

static bool emmc_card_reset(void) {
	LOG_DEBUG("resetting card");

	EMMC->control[1] = CTRL1_RESET_HOST;

	TRY_MSG(wait_reg_mask(&EMMC->control[1], CTRL1_RESET_ALL, false, TIMEOUT_DEFAULT))

	enable_bus_power();

	TRY_MSG(emmc_setup_clock())

	EMMC->interrupt_enable = 0;
	EMMC->interrupt_flags = 0xffffffff;
	EMMC->interrupt_mask = 0xffffffff;

	device.transfer_blocks = 0;

	TRY_MSG(emmc_command(command_go_idle, 0, TIMEOUT_DEFAULT))

	bool v2_card = check_v2_card();

	TRY_MSG(check_usable_card())

	TRY_MSG(check_ocr())

	TRY_MSG(check_sdhc_support(v2_card))

	switch_clock_rate(device.base_clock, SD_CLOCK_NORMAL);

	TRY_MSG(check_rca())

	TRY_MSG(check_card_specific_data())

	TRY_MSG(select_card())

	if (!device.high_capacity) {
		TRY_MSG(emmc_command(command_set_block_length, EMMC_BLOCK_SIZE, TIMEOUT_DEFAULT))
	}

	// Acknowledge any leftover interrupts, just to be safe.
	EMMC->interrupt_flags = 0xffffffff;

	return true;
}

static bool do_data_command(bool const write, union read_or_write const buffer, u32 const num_blocks, u32 block_start) {
	if (num_blocks == 0) {
		LOG_DEBUG("data command with 0 blocks, returning early");
		return true;
	}

	if (!device.high_capacity) {
		block_start *= EMMC_BLOCK_SIZE;
	}

	device.transfer_blocks = num_blocks;
	device.buffer = buffer;

	static u32 COMMANDS[2][2] = {
		{ command_read_block, command_read_multiple },
		{ command_write_block, command_write_multiple },
	};
	u32 const command = COMMANDS[write][num_blocks > 1];

	return emmc_command(command, block_start, 5'000);
}

// Unaligned accesses are fine.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
bool emmc_read(u8* const buffer, u32 const start_block, u32 const num_blocks) {
	LOG_DEBUG("reading %u blocks starting at %u (0x%x)", num_blocks, start_block, start_block);
	return do_data_command(false, (union read_or_write){ .read = (u32*)buffer }, num_blocks, start_block);
}

bool emmc_write(u8 const* const buffer, u32 const start_block, u32 const num_blocks) {
	LOG_DEBUG("writing %u blocks starting at %u (0x%x)", num_blocks, start_block, start_block);
	return do_data_command(true, (union read_or_write){ .write = (u32 const*)buffer }, num_blocks, start_block);
}
#pragma GCC diagnostic pop

u32 emmc_capacity(void) {
	return device.capacity;
}

bool emmc_init(void) {
	LOG_DEBUG("initializing EMMC");

	gpio_set_mode(34, gpio_mode_input);
	gpio_set_mode(35, gpio_mode_input);
	gpio_set_mode(36, gpio_mode_input);
	gpio_set_mode(37, gpio_mode_input);
	gpio_set_mode(38, gpio_mode_input);
	gpio_set_mode(39, gpio_mode_input);

	gpio_set_mode(48, gpio_mode_alt3);
	gpio_set_mode(49, gpio_mode_alt3);
	gpio_set_mode(50, gpio_mode_alt3);
	gpio_set_mode(51, gpio_mode_alt3);
	gpio_set_mode(52, gpio_mode_alt3);

	return emmc_card_reset();
}
