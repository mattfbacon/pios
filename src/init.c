#include "gpio.h"
#include "uart.h"

void standard_init(void) {
	uart_init();

	gpio_set_mode(42, gpio_mode_output);
	gpio_write(42, true);
}
