#include "base.h"
#include "gpio.h"
#include "i2c.h"

static struct {
	u32 control;
	u32 status;
	u32 data_length;
	u32 peer_address;
	u32 fifo;
	u32 div;
	u32 delay;
	u32 clock_stretch;
} volatile* const BASE = (void volatile*)(PERIPHERAL_BASE + 0x804000);

enum {
	CONTROL_READ = 1 << 0,
	CONTROL_CLEAR_FIFO = 1 << 4,
	CONTROL_START_TRANSFER = 1 << 7,
	CONTROL_ENABLE = 1 << 15,

	STATUS_DONE = 1 << 1,
	STATUS_CAN_WRITE = 1 << 4,
	STATUS_CAN_READ = 1 << 5,
	STATUS_ACK_ERROR = 1 << 8,
	STATUS_CLOCK_STRETCH_TIMEOUT = 1 << 9,
};

void i2c_init(u32 const clock_speed) {
	gpio_set_mode(2, gpio_mode_alt0);
	gpio_set_pull(2, gpio_pull_down);

	gpio_set_mode(3, gpio_mode_alt0);
	gpio_set_pull(3, gpio_pull_down);

	BASE->div = CORE_CLOCK_SPEED / clock_speed;
}

bool i2c_recv(i2c_address_t const address, u8* const buf, u32 const len) {
	BASE->peer_address = (u32)address;
	BASE->control = CONTROL_CLEAR_FIFO;
	// clear all these flags before the transfer
	BASE->status = STATUS_DONE | STATUS_ACK_ERROR | STATUS_CLOCK_STRETCH_TIMEOUT;
	BASE->data_length = len;
	BASE->control = CONTROL_ENABLE | CONTROL_START_TRANSFER | CONTROL_READ;

	u32 amount_read = 0;

	while (amount_read < len && !(BASE->status & STATUS_DONE)) {
		while (amount_read < len && (BASE->status & STATUS_CAN_READ)) {
			buf[amount_read] = BASE->fifo;
			++amount_read;
		}
		asm volatile("isb");
	}

	u32 const status = BASE->status;
	BASE->status = STATUS_DONE;
	return !(status & STATUS_ACK_ERROR) && !(status & STATUS_CLOCK_STRETCH_TIMEOUT) && amount_read == len;
}

bool i2c_send(i2c_address_t const address, u8 const* const buf, u32 const len) {
	BASE->peer_address = (u32)address;
	BASE->control = CONTROL_CLEAR_FIFO;
	// clear all these flags before the transfer
	BASE->status = STATUS_DONE | STATUS_ACK_ERROR | STATUS_CLOCK_STRETCH_TIMEOUT;
	BASE->data_length = len;
	BASE->control = CONTROL_ENABLE | CONTROL_START_TRANSFER;

	u32 written = 0;

	while (written < len && !(BASE->status & STATUS_DONE)) {
		while (written < len && (BASE->status & STATUS_CAN_WRITE)) {
			BASE->fifo = buf[written];
			++written;
		}
		asm volatile("isb");
	}

	u32 const status = BASE->status;
	BASE->status = STATUS_DONE;
	return !(status & STATUS_ACK_ERROR) && !(status & STATUS_CLOCK_STRETCH_TIMEOUT) && written == len;
}
