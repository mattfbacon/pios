#include "devices/mcp23017.h"
#include "sleep.h"
#include "uart.h"

// Unfortunately, for almost all the registers here, we have to get them before we can set them, since the MCP23017 doesn't use set/clear registers like the
// BCM2711 GPIO. However, the overhead of the double transfer is fairly trivial.

enum {
	REG_PIN_MODE = 0x00,
	REG_POLARITY = 0x02,
	REG_PULL_UP = 0x0c,
	REG_GPIO = 0x12,
	REG_OUTPUT_LATCH = 0x14,
};

bool mcp23017_init(i2c_address_t) {
	return true;
}

// 8 pins per port

static u8 offset_for_pin(mcp23017_pin_t const pin) {
	return pin / 8;
}

static u8 shift_for_pin(mcp23017_pin_t const pin) {
	return pin % 8;
}

#define TRY(_inner) \
	if (!(_inner)) { \
		return false; \
	}

// This delay was determined from testing.
// I'm not sure if there is any official information to this end in the datasheet.
static void io_delay(void) {
	sleep_micros(250);
}

static bool register_read(i2c_address_t const address, u8 const reg, u8* const ret) {
	io_delay();
	TRY(i2c_send(address, &reg, 1))
	TRY(i2c_recv(address, ret, 1))
	return true;
}

static bool register_write(i2c_address_t const address, u8 const reg, u8 const value) {
	io_delay();
	u8 const buf[2] = { reg, value };
	return i2c_send(address, buf, sizeof(buf));
}

static bool register_set_bit(i2c_address_t const address, u8 const in_reg, u8 const out_reg, u8 const bit, bool const set) {
	u8 reg_value;
	TRY(register_read(address, in_reg, &reg_value))

	u8 const mask = 1 << bit;
	reg_value &= ~mask;
	reg_value |= (u8)set << bit;

	TRY(register_write(address, out_reg, reg_value))

	return true;
}

static bool register_set_pin_bit(i2c_address_t const address, u8 const in_reg, u8 const out_reg, mcp23017_pin_t const pin, bool const bit) {
	u8 const pin_offset = offset_for_pin(pin);
	return register_set_bit(address, in_reg + pin_offset, out_reg + pin_offset, shift_for_pin(pin), bit);
}

static bool register_get_pin_bit(i2c_address_t const address, u8 const reg_offset, mcp23017_pin_t const pin, bool* const ret) {
	u8 value;
	TRY(register_read(address, reg_offset + offset_for_pin(pin), &value))
	*ret = value & (1 << shift_for_pin(pin));
	return true;
}

bool mcp23017_set_mode(i2c_address_t const address, mcp23017_pin_t const pin, mcp23017_mode_t const mode) {
	return register_set_pin_bit(address, REG_PIN_MODE, REG_PIN_MODE, pin, (bool)mode);
}

bool mcp23017_write(i2c_address_t const address, mcp23017_pin_t const pin, bool const value) {
	return register_set_pin_bit(address, REG_OUTPUT_LATCH, REG_GPIO, pin, value);
}

bool mcp23017_write_all(i2c_address_t const address, u16 const value) {
	io_delay();
	// rely on auto-increment to write to both banks
	u8 const buf[] = { REG_GPIO, value & 0xff, value >> 8 };
	return i2c_send(address, buf, sizeof(buf));
}

bool mcp23017_read(i2c_address_t const address, mcp23017_pin_t const pin, bool* const ret) {
	return register_get_pin_bit(address, REG_GPIO, pin, ret);
}

bool mcp23017_read_all(i2c_address_t address, u16* ret) {
	io_delay();
	// rely on auto-increment to read from both banks
	u8 const reg = REG_GPIO;
	TRY(i2c_send(address, &reg, sizeof(reg)))
	return i2c_recv(address, (u8*)ret, sizeof(*ret));
}

bool mcp23017_set_pull(i2c_address_t const address, mcp23017_pin_t const pin, mcp23017_pull_t const pull) {
	return register_set_pin_bit(address, REG_PULL_UP, REG_PULL_UP, pin, (bool)pull);
}
