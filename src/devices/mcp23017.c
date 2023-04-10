// We use a persistent struct to allow optimizing operations such as setting a single pin.
// Otherwise we would have to fetch the current state, modify it, then send it back.
// Especially in the somewhat timing-sensitive context of the LCD display, that approach would be far too slow.
//
// This approach extends to all operations including "write all", which will intelligently determine which register(s) must be written, if any.
//
// # References
//
// - <https://ww1.microchip.com/downloads/en/devicedoc/20001952c.pdf>
// - <https://github.com/adafruit/Adafruit-RGB-LCD-Shield-Library/blob/master/utility/Adafruit_MCP23017.cpp>
// - <https://github.com/adafruit/Adafruit-MCP23017-Arduino-Library/blob/master/src/Adafruit_MCP23X17.cpp>

#include "devices/mcp23017.h"
#include "sleep.h"
#include "try.h"
#include "uart.h"

enum : u8 {
	REG_PIN_MODE = 0x00,
	REG_POLARITY = 0x02,
	REG_PULL_UP = 0x0c,
	REG_GPIO = 0x12,
	REG_OUTPUT_LATCH = 0x14,
};

// 8 pins per port.
static u8 offset_for_pin(mcp23017_pin_t const pin) {
	return pin / 8;
}

static u8 shift_for_pin(mcp23017_pin_t const pin) {
	return pin % 8;
}

// This delay was determined from testing.
// I'm not sure if there is any official information to this end in the datasheet.
static void io_delay(void) {
	sleep_micros(250);
}

static bool read_u8(i2c_address_t const address, u8 const reg, u8* const value) {
	TRY(i2c_send(address, &reg, 1))
	io_delay();
	return i2c_recv(address, value, 1);
}

static bool read_u16(i2c_address_t const address, u8 const reg, u16* const value) {
	TRY(i2c_send(address, &reg, 1))
	io_delay();
	return i2c_recv(address, (u8*)value, sizeof(*value));
}

static bool write_u8(i2c_address_t const address, u8 const reg, u8 const value) {
	u8 const buf[2] = { reg, value };
	return i2c_send(address, buf, sizeof(buf));
}

static bool write_u16(i2c_address_t const address, u8 const reg, u16 const value) {
	u8 const buf[3] = { reg, value & 0xff, value >> 8 };
	return i2c_send(address, buf, sizeof(buf));
}

static bool write_reg(i2c_address_t const address, u8 const reg_base, u8* const storage_base, mcp23017_pin_t const pin, bool const value) {
	u8 const offset = offset_for_pin(pin);
	u8 const shift = shift_for_pin(pin);

	u8* reg = storage_base + offset;
	u8 new_value = *reg;
	new_value &= ~(1 << shift);
	new_value |= (u8)value << shift;
	if (new_value == *reg) {
		return true;
	}
	*reg = new_value;
	return write_u8(address, reg_base + offset, *reg);
}

static bool read_reg(i2c_address_t const address, u8 const reg_base, mcp23017_pin_t const pin, bool* const ret) {
	u8 const offset = offset_for_pin(pin);
	u8 const shift = shift_for_pin(pin);

	u8 value;
	TRY(read_u8(address, reg_base + offset, &value))
	*ret = value & (1 << shift);
	return true;
}

void mcp23017_init(mcp23017_device_t* const this, i2c_address_t const address) {
	LOG_DEBUG("initializing MCP23017 object");

	this->address = address;
	this->pulls[0] = 0x00;
	this->pulls[1] = 0x00;
	this->modes[0] = 0xff;
	this->modes[1] = 0xff;
	this->outputs[0] = 0x00;
	this->outputs[1] = 0x00;
}

bool mcp23017_set_mode(mcp23017_device_t* const this, mcp23017_pin_t const pin, mcp23017_mode_t const mode) {
	LOG_DEBUG("setting mode of pin %u to %u", pin, mode);

	io_delay();
	return write_reg(this->address, REG_PIN_MODE, this->modes, pin, (bool)mode);
}

bool mcp23017_write(mcp23017_device_t* const this, mcp23017_pin_t const pin, bool const value) {
	LOG_DEBUG("writing %@b to pin %u", value, pin);

	io_delay();
	return write_reg(this->address, REG_GPIO, this->outputs, pin, value);
}

bool mcp23017_write_all(mcp23017_device_t* const this, u16 const values) {
	LOG_DEBUG("writing all pins: %x", values);

	io_delay();

	u8 const old_bank0 = this->outputs[0];
	u8 const old_bank1 = this->outputs[1];
	u8 const new_bank0 = (u8)(values & 0xff);
	u8 const new_bank1 = (u8)(values >> 8);

	this->outputs[0] = new_bank0;
	this->outputs[1] = new_bank1;

	if (new_bank0 != old_bank0) {
		if (new_bank1 != old_bank1) {
			return write_u16(this->address, REG_GPIO, (u16)(new_bank1 << 8) | new_bank0);
		} else {
			return write_u8(this->address, REG_GPIO, new_bank0);
		}
	} else {
		if (new_bank1 != old_bank1) {
			return write_u8(this->address, REG_GPIO + 1, new_bank1);
		} else {
			return true;
		}
	}
}

bool mcp23017_read(mcp23017_device_t* const this, mcp23017_pin_t const pin, bool* const ret) {
	LOG_DEBUG("reading pin %u", pin);

	io_delay();
	TRY(read_reg(this->address, REG_GPIO, pin, ret))
	LOG_DEBUG("read %@b from pin %u", *ret, pin);
	return true;
}

bool mcp23017_read_all(mcp23017_device_t* const this, u16* const ret) {
	LOG_DEBUG("reading all pins");

	io_delay();
	TRY(read_u16(this->address, REG_GPIO, ret))
	LOG_DEBUG("read all pins: %x", *ret);
	return true;
}

bool mcp23017_set_pull(mcp23017_device_t* const this, mcp23017_pin_t const pin, mcp23017_pull_t const pull) {
	LOG_DEBUG("setting pull of pin %u to %u", pin, pull);

	io_delay();
	return write_reg(this->address, REG_PULL_UP, this->pulls, pin, (bool)pull);
}
