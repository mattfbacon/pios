#include "gpio.h"
#include "sleep.h"

void kernel_main(void) {
	gpio_pin_t const act_led_pin = 42;
	gpio_set_mode(act_led_pin, gpio_mode_output);
	while (true) {
		gpio_write(act_led_pin, true);
		sleep_micros(500000);
		gpio_write(act_led_pin, false);
		sleep_micros(500000);
	}
}
