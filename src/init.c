#include "exception.h"
#include "gpio.h"
#include "uart.h"

void standard_init(void) {
	exception_init();

	uart_init();
	// clear the terminal
	uart_send_str("\e[2J\e[H");

	gpio_set_mode(42, gpio_mode_output);
	gpio_write(42, true);
}
