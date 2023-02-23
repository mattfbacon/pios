#include "devices/lcd.h"
#include "i2c.h"
#include "init.h"
#include "sleep.h"

typedef enum mode {
	mode_initial,
	mode_select,
	mode_right,
	mode_down,
	mode_up,
	mode_left,
} mode_t;

typedef enum color {
	color_black = 0,
	color_red = 1 << 2,
	color_green = 1 << 1,
	color_blue = 1 << 0,
	color_yellow = color_red | color_green,
	color_teal = color_green | color_blue,
	color_purple = color_red | color_blue,
	color_white = color_red | color_green | color_blue,
} color_t;

static void set_backlight(color_t const color) {
	lcd_set_backlight(color & color_red, color & color_green, color & color_blue);
}

static void set_mode(mode_t const mode) {
	color_t color = color_red;
	switch (mode) {
		case mode_initial:
			color = color_white;
			break;
		case mode_select:
			color = color_blue;
			break;
		case mode_right:
			color = color_green;
			break;
		case mode_down:
			color = color_purple;
			break;
		case mode_up:
			color = color_teal;
			break;
		case mode_left:
			color = color_yellow;
			break;
	}
	set_backlight(color);

	lcd_set_position(0, 0);

	char const* text = "???";
	switch (mode) {
		case mode_initial:
			text = "initial";
			break;
		case mode_select:
			text = "select";
			break;
		case mode_right:
			text = "right";
			break;
		case mode_down:
			text = "down";
			break;
		case mode_up:
			text = "up";
			break;
		case mode_left:
			text = "left";
			break;
	}

	lcd_print_line(text);
}

void main(void) {
	i2c_init(i2c_speed_standard);

	lcd_init();

	set_backlight(color_white);

	mode_t mode = mode_initial;
	set_mode(mode);
	while (true) {
		mode_t new_mode = mode;
		lcd_buttons_t const buttons = lcd_get_buttons();

		if (buttons & lcd_button_select) {
			new_mode = mode_select;
		} else if (buttons & lcd_button_right) {
			new_mode = mode_right;
		} else if (buttons & lcd_button_down) {
			new_mode = mode_down;
		} else if (buttons & lcd_button_up) {
			new_mode = mode_up;
		} else if (buttons & lcd_button_left) {
			new_mode = mode_left;
		}

		// Comparing strings by pointer is generally bad and `strcmp` should be preferred, but in this case the possible strings are a closed set so the behavior is the same.
		if (new_mode != mode) {
			mode = new_mode;
			set_mode(mode);
		}

		sleep_micros(1'000);
	}
}
