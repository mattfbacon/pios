// Allows interfacing with the HD44780U LCD controller via the MCP23017 I2C GPIO expander chip.
//
// The HD44780U device has many configuration options such as one- vs two-line operation, 5x8 vs 5x10 characters, left-to-right vs right-to-left text, etc.
// For the sake of simplicity, this driver is opinionated and chooses two-line operation, 5x8 characters, and left-to-right text.
//
// # References
//
// - <https://www.sparkfun.com/datasheets/LCD/HD44780.pdf>

#pragma once

typedef enum lcd_direction {
	lcd_direction_left = 0,
	lcd_direction_right = 1,
} lcd_direction_t;

typedef struct lcd_position {
	// Zero-indexed.
	u8 line;
	// Zero-indexed.
	u8 column;
} lcd_position_t;

typedef enum lcd_buttons {
	lcd_button_select = 1 << 0,
	lcd_button_right = 1 << 1,
	lcd_button_down = 1 << 2,
	lcd_button_up = 1 << 3,
	lcd_button_left = 1 << 4,
} lcd_buttons_t;

// Don't forget to initialize I2C.
// This function handles initializing the MCP23017 expander, though.
//
// This will also clear and turn on the display, equivalent to `lcd_clear(); lcd_set_display(true, false, false);`.
void lcd_init(void);

void lcd_set_backlight(bool red, bool green, bool blue);

// Also resets the position to (0, 0).
void lcd_clear(void);

// Resets the position to (0, 0) and resets any display shifting.
//
// This command is significantly slower than simply calling `lcd_set_position(0, 0)` so only use it if you also need to reset the display shifting.
void lcd_go_home(void);

void lcd_set_display(bool on, bool cursor, bool blink);

void lcd_shift_cursor(lcd_direction_t direction);

void lcd_shift_display(lcd_direction_t direction);

// Both lines and columns are zero-indexed.
void lcd_set_position(u8 line, u8 column);

// Each character is 5 columns by 8 rows.
// The 5 LSBs in each item of `data` indicate the on/off status of each pixel in the character.
//
// `index` can be 0..8, allowing for 8 custom characters.
// To use a custom character, use an ASCII character in the range 0..16.
// The range 8..16 mirrors the range 0..8.
// To avoid having the first custom character be an ASCII NUL and cause problems, I suggest using the upper range, 8..16.
//
// This will reset the position to (0, 0) as a side effect.
// Use `lcd_set_position` to restore it if necessary.
//
// # Examples
//
// ```c
// static u8 const degree_character[8] = { 0b01100, 0b10010, 0b10010, 0b01100, 0, 0, 0, 0 };
// lcd_load_character(0, degree_character);
// lcd_print_str("Temp: 10\8C");
// ```
//
void lcd_load_character(u8 index, u8 const data[8]);

lcd_buttons_t lcd_get_buttons(void);

// For more information about special characters (0x80..), see the datasheet linked in the module documentation, page 17 or 18 depending on your ROM.
void lcd_print(char ch);
void lcd_print_str(char const* str);
// Prints spaces up to 16 characters to ensure that the rest of the line is empty.
// Does not go to the next line automatically.
void lcd_print_line(char const* str);
void lcd_printf(char const* fmt, ...);
void lcd_vprintf(char const* fmt, __builtin_va_list args);
