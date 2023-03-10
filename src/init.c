#include "exception.h"
#include "gpio.h"
#include "log.h"
#include "malloc.h"
#include "uart.h"

void standard_init(void) {
	exception_init();

	uart_init();
	// clear the terminal
	uart_send_str("\e[2J\e[H");

	malloc_init();

	gpio_set_mode(42, gpio_mode_output);
	gpio_write(42, true);

	LOG_INFO("hello, world! system is up.");
}
