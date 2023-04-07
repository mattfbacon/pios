#include "exception.h"
#include "gpio.h"
#include "log.h"
#include "malloc.h"
#include "uart.h"

// Only exposed to assembly.
void standard_init(void);

void standard_init(void) {
	exception_init();

	uart_init();
	// Clear the terminal.
	uart_send_str("\e[2J\e[H");

	malloc_init();

	// Turn on the ACT LED to indicate that the system is up.
	gpio_set_mode(42, gpio_mode_output);
	gpio_write(42, true);

	LOG_INFO("hello, world! system is up.");
}
