#include "devices/mcp23017.h"

// Unfortunately, for almost all the registers here, we have to get them before we can set them, since the MCP23017 doesn't use set/clear registers like the
// BCM2711 GPIO. However, the overhead of the double transfer is fairly trivial.

// This driver assumes that the IOCON register starts off in its reset state, which is all zeros. As a reminder, this means that:
// - Registers are sequential, so IODIRB is immediately after IODIRA.
// - Sequential operation is enabled, so the address pointer automatically increments.
// However, our init function will toggle both of these, so:
// - Registers are segregated, so all the GPIOA registers come before the GPIOB registers. However, in software the difference is just adding 0x10 instead of 0x1.
// - Sequential operation is disabled, so the address pointer stays the same.

enum {
	// `IOCON` when in `IOCON.BANK=0` mode.
	REGINBANK0_IOCON = 0x0a,

	REG_PIN_MODE = 0x00,
	REG_POLARITY = 0x01,
	REG_PULL_UP = 0x06,
	REG_READ = 0x09,
	REG_WRITE = 0x0a,

	IOCON_BYTE_MODE = 1 << 5,
	IOCON_SETBANK1 = 1 << 7,
};

bool mcp23017_init(i2c_address_t const address) {
	u8 const message[] = {
		REGINBANK0_IOCON,
		IOCON_BYTE_MODE | IOCON_SETBANK1,
	};
	return i2c_send(address, message, sizeof(message));
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

static bool register_read(i2c_address_t const address, u8 const reg, u8* const ret) {
	TRY(i2c_send(address, &reg, 1))
	TRY(i2c_recv(address, ret, 1))
	return true;
}

static bool register_write(i2c_address_t const address, u8 const reg, u8 const value) {
	u8 const buf[2] = { reg, value };
	return i2c_send(address, buf, sizeof(buf));
}

static bool register_set_bit(i2c_address_t const address, u8 const reg, u8 const bit, bool const set) {
	u8 reg_value;
	TRY(register_read(address, reg, &reg_value))

	u8 const mask = 1 << bit;
	reg_value &= ~mask;
	reg_value |= (u8)set << bit;

	TRY(register_write(address, reg, reg_value))

	return true;
}

static bool register_set_pin_bit(i2c_address_t const address, u8 const reg_offset, mcp23017_pin_t const pin, bool const bit) {
	return register_set_bit(address, reg_offset + offset_for_pin(pin), shift_for_pin(pin), bit);
}

static bool register_get_pin_bit(i2c_address_t const address, u8 const reg_offset, mcp23017_pin_t const pin, bool* const ret) {
	u8 value;
	TRY(register_read(address, reg_offset + offset_for_pin(pin), &value))
	*ret = value & (1 << shift_for_pin(pin));
	return true;
}

bool mcp23017_set_mode(i2c_address_t const address, mcp23017_pin_t const pin, mcp23017_mode_t const mode) {
	return register_set_pin_bit(address, REG_PIN_MODE, pin, (bool)mode);
}

bool mcp23017_write(i2c_address_t const address, mcp23017_pin_t const pin, bool const value) {
	return register_set_pin_bit(address, REG_WRITE, pin, value);
}

bool mcp23017_read(i2c_address_t const address, mcp23017_pin_t const pin, bool* const ret) {
	return register_get_pin_bit(address, REG_READ, pin, ret);
}

bool mcp23017_set_pull(i2c_address_t const address, mcp23017_pin_t const pin, mcp23017_pull_t const pull) {
	return register_set_pin_bit(address, REG_PULL_UP, pin, (bool)pull);
}
