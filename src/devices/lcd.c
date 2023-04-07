// # Implementation Notes
//
// The device runs in 4-bit mode due to scarcity of GPIO lines on the MCP23017.
//
// # References
//
// - <https://github.com/adafruit/Adafruit-RGB-LCD-Shield-Library>
// - <https://www.sparkfun.com/datasheets/LCD/HD44780.pdf>

#include "devices/lcd.h"
#include "devices/mcp23017.h"
#include "log.h"
#include "printf.h"
#include "sleep.h"

enum {
	PIN_BACKLIGHT_RED = 6,
	PIN_BACKLIGHT_GREEN = 7,
	PIN_BACKLIGHT_BLUE = 8,

	// Low for instruction register, high for data register.
	PIN_REG_SELECT = 15,
	// Low for write, high for read.
	PIN_READ_WRITE = 14,
	PIN_ENABLE = 13,

	// Bits 0 and 4.
	PIN_DATA0 = 12,
	// Bits 1 and 5.
	PIN_DATA1 = 11,
	// Bits 2 and 6.
	PIN_DATA2 = 10,
	// Bits 3 and 7.
	PIN_DATA3 = 9,

	PIN_BUSY = PIN_DATA3,

	PIN_BUTTON_SELECT = 0,
	PIN_BUTTON_RIGHT = 1,
	PIN_BUTTON_DOWN = 2,
	PIN_BUTTON_UP = 3,
	PIN_BUTTON_LEFT = 4,
};

static mcp23017_pin_t const data_pins[] = { PIN_DATA0, PIN_DATA1, PIN_DATA2, PIN_DATA3 };

static mcp23017_pin_t const outputs[] = {
	PIN_BACKLIGHT_RED, PIN_BACKLIGHT_GREEN, PIN_BACKLIGHT_BLUE, PIN_REG_SELECT, PIN_READ_WRITE, PIN_ENABLE, PIN_DATA0, PIN_DATA1, PIN_DATA2, PIN_DATA3,
};

static mcp23017_pin_t const inputs[] = {
	PIN_BUTTON_SELECT, PIN_BUTTON_RIGHT, PIN_BUTTON_DOWN, PIN_BUTTON_UP, PIN_BUTTON_LEFT,
};

enum {
	COMMAND_CLEAR = 0x01,

	COMMAND_GO_HOME = 0x02,

	COMMAND_SET_ENTRY_MODE = 0x04,
	// Unset is right-to-left.
	MODE_LEFT_TO_RIGHT = 0x02,
	// Unset is left-justify.
	MODE_RIGHT_JUSTIFY = 0x01,

	COMMAND_SET_DISPLAY = 0x08,
	DISPLAY_ON = 0x04,
	DISPLAY_CURSOR = 0x02,
	DISPLAY_BLINK = 0x01,

	COMMAND_SHIFT = 0x10,
	// Unset shifts cursor.
	SHIFT_DISPLAY = 0x08,

	COMMAND_SET_FUNCTION = 0x20,
	FUNCTION_2LINE = 0x08,

	COMMAND_SET_CGRAM_ADDRESS = 0x40,

	COMMAND_SET_DDRAM_ADDRESS = 0x80,

	BUTTON_MASK = 0b11111,
	BACKLIGHT_MASK = (1 << PIN_BACKLIGHT_RED) | (1 << PIN_BACKLIGHT_GREEN) | (1 << PIN_BACKLIGHT_BLUE),
};

static mcp23017_device_t device;

static void set_mode(mcp23017_pin_t const pin, mcp23017_mode_t const mode) {
	mcp23017_set_mode(&device, pin, mode);
}

static void set_pull(mcp23017_pin_t const pin, mcp23017_pull_t const pull) {
	mcp23017_set_pull(&device, pin, pull);
}

static void write(mcp23017_pin_t const pin, bool const value) {
	mcp23017_write(&device, pin, value);
}

static void write_all(u16 const values) {
	mcp23017_write_all(&device, values);
}

static bool read(mcp23017_pin_t const pin) {
	bool ret = false;
	mcp23017_read(&device, pin, &ret);
	return ret;
}

static u16 read_all(void) {
	u16 ret = 0;
	mcp23017_read_all(&device, &ret);
	return ret;
}

static void pulse_enable(void) {
	write(PIN_ENABLE, true);
	sleep_micros(1);
	write(PIN_ENABLE, false);
	sleep_micros(1);
}

static void send4(u8 const value) {
	u16 outputs = (((u16)device.outputs[1]) << 8) | (u16)device.outputs[0];

	for (u16 i = 0; i < 4; ++i) {
		u16 const pin = data_pins[i];
		u16 const output_bit = 1 << pin;
		outputs &= ~output_bit;
		outputs |= (u16)(bool)(value & (1 << i)) << pin;
	}

	write_all(outputs);

	sleep_micros(100);
	pulse_enable();
}

static void wait_until_not_busy(void) {
	write(PIN_REG_SELECT, false);
	write(PIN_READ_WRITE, true);
	set_mode(PIN_BUSY, mcp23017_mode_input);
	// Don't wait more than about 100 ms.
	for (u32 i = 0; i < 100; ++i) {
		write(PIN_ENABLE, true);
		sleep_micros(1);
		bool const busy = read(PIN_BUSY);
		write(PIN_ENABLE, false);
		sleep_micros(1);
		pulse_enable();

		if (!busy) {
			break;
		}

		sleep_micros(SLEEP_MIN_MICROS_FOR_INTERRUPTS);
	}
	set_mode(PIN_BUSY, mcp23017_mode_output);
}

static void send8(u8 const value, bool const reg_select) {
	write(PIN_REG_SELECT, reg_select);
	write(PIN_READ_WRITE, false);
	send4(value >> 4);
	send4(value);
	wait_until_not_busy();
}

static void send_command(u8 const value) {
	send8(value, false);
}

static void send_data(u8 const value) {
	send8(value, true);
}

void lcd_init() {
	LOG_DEBUG("initializing LCD");

	mcp23017_init(&device, MCP23017_ADDRESS);

	LOG_TRACE("setting output modes");

	for (usize i = 0; i < sizeof(outputs) / sizeof(outputs[0]); ++i) {
		set_mode(outputs[i], mcp23017_mode_output);
	}

	LOG_TRACE("setting input modes");

	for (usize i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
		set_mode(inputs[i], mcp23017_mode_input);
		set_pull(inputs[i], mcp23017_pull_up);
	}

	LOG_TRACE("starting initialization handshake");

	write(PIN_ENABLE, false);

	write(PIN_REG_SELECT, false);
	write(PIN_READ_WRITE, false);

	send4(0x3);
	sleep_micros(4'500);

	send4(0x3);
	sleep_micros(150);

	send4(0x3);
	sleep_micros(1'000);

	send4(0x2);

	LOG_TRACE("handshake done, setting defaults");

	send_command(COMMAND_SET_FUNCTION | FUNCTION_2LINE);

	send_command(COMMAND_SET_ENTRY_MODE | MODE_LEFT_TO_RIGHT);

	lcd_clear();

	lcd_set_display(true, false, false);
}

void lcd_set_backlight(bool const red, bool const green, bool const blue) {
	LOG_DEBUG("setting backlight to red %@b green %@b blue %@b", red, green, blue);

	// This is less for optimization and more for avoiding flickering when setting the backlight.
	u16 pins = (u16)device.outputs[1] << 8 | device.outputs[0];
	pins &= ~BACKLIGHT_MASK;
	pins |= (u16)(!red) << PIN_BACKLIGHT_RED | (u16)(!green) << PIN_BACKLIGHT_GREEN | (u16)(!blue) << PIN_BACKLIGHT_BLUE;
	write_all(pins);
}

void lcd_clear(void) {
	LOG_DEBUG("clearing display");

	send_command(COMMAND_CLEAR);
}

void lcd_go_home(void) {
	LOG_DEBUG("going home");

	send_command(COMMAND_GO_HOME);
}

void lcd_set_display(bool const on, bool const cursor, bool const blink) {
	LOG_DEBUG("setting display flags: on=%@b cursor=%@b blink=%@b", on, cursor, blink);

	u8 command = COMMAND_SET_DISPLAY;
	if (on) {
		command |= DISPLAY_ON;
	}
	if (cursor) {
		command |= DISPLAY_CURSOR;
	}
	if (blink) {
		command |= DISPLAY_BLINK;
	}
	send_command(command);
}

void lcd_shift_cursor(lcd_direction_t const direction) {
	LOG_DEBUG("shifting cursor in direction", direction ? "right" : "left");

	send_command(COMMAND_SHIFT | ((u8)direction << 2));
}

void lcd_shift_display(lcd_direction_t const direction) {
	LOG_DEBUG("shifting display in direction", direction ? "right" : "left");

	send_command(COMMAND_SHIFT | SHIFT_DISPLAY | ((u8)direction << 2));
}

void lcd_set_position(u8 const line, u8 const column) {
	LOG_DEBUG("setting position to line %u column %u", line, column);

	if (line >= 2 || column >= 40) {
		LOG_WARN("line or column out of range");
		return;
	}
	u8 const position = line * 0x40 | column;
	send_command(COMMAND_SET_DDRAM_ADDRESS | position);
}

void lcd_load_character(u8 const index, u8 const data[8]) {
	LOG_DEBUG("loading custom character %u", index);

	if (index >= 8) {
		LOG_WARN("index out of range");
		return;
	}

	send_command(COMMAND_SET_CGRAM_ADDRESS | (index * 8));

	for (usize i = 0; i < 8; ++i) {
		send_data(data[i]);
	}

	lcd_set_position(0, 0);
}

lcd_buttons_t lcd_get_buttons() {
	LOG_DEBUG("getting buttons");

	// In hardware the buttons are active-low, so do a bitwise-not.
	lcd_buttons_t const values = ~(read_all() & BUTTON_MASK);
	LOG_DEBUG("buttons state: %x", values);
	return values;
}

void lcd_print(char const ch) {
	LOG_DEBUG("printing character %c (%b)", ch, ch);
	send_data(ch);
}

void lcd_print_str(char const* str) {
	for (; *str; ++str) {
		lcd_print(*str);
	}
}

void lcd_print_line(char const* str) {
	u32 i = 0;
	for (; i < 16 && *str; ++str, ++i) {
		lcd_print(*str);
	}
	for (; i < 16; ++i) {
		lcd_print(' ');
	}
}

void lcd_printf(char const* fmt, ...) {
	__builtin_va_list args;
	__builtin_va_start(args, fmt);
	lcd_vprintf(fmt, args);
	__builtin_va_end(args);
}

struct buf_writer {
	char line[40];
	usize idx;
};

static void write_buf(void* const user, char const ch) {
	struct buf_writer* const buf = user;
	if (buf->idx >= sizeof(buf->line)) {
		return;
	}
	buf->line[buf->idx] = ch;
	++buf->idx;
}

void lcd_vprintf(char const* fmt, __builtin_va_list args) {
	struct buf_writer buf = {};

	vdprintf(write_buf, (void*)&buf, fmt, args);

	for (usize i = 0; i < buf.idx; ++i) {
		send_data(buf.line[i]);
	}
	for (usize i = buf.idx; i < 16; ++i) {
		send_data(' ');
	}
}
