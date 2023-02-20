#include "devices/lcd.h"
#include "i2c.h"
#include "init.h"
#include "sleep.h"

void main(void) {
	standard_init();

	i2c_init(i2c_speed_standard);

	lcd_init();

	lcd_set_backlight(true, false, true);

	char const* mode = "no";
	lcd_print_line(mode);
	while (true) {
		char const* new_mode = mode;
		lcd_buttons_t const buttons = lcd_get_buttons();

		if (buttons & lcd_button_select) {
			new_mode = "select";
		} else if (buttons & lcd_button_right) {
			new_mode = "right";
		} else if (buttons & lcd_button_down) {
			new_mode = "down";
		} else if (buttons & lcd_button_up) {
			new_mode = "up";
		} else if (buttons & lcd_button_left) {
			new_mode = "left";
		}

		// Comparing strings by pointer is generally bad and `strcmp` should be preferred, but in this case the possible strings are a closed set so the behavior is the same.
		if (new_mode != mode) {
			mode = new_mode;

			lcd_set_position(0, 0);
			lcd_print_line(mode);
		}

		sleep_micros(1'000);
	}
}
