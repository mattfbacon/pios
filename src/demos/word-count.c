#include "gpio.h"
#include "string.h"
#include "uart.h"

void main(void) {
	uart_init();

	gpio_set_mode(42, gpio_mode_output);
	gpio_write(42, true);

	while (true) {
		uart_send_str("Enter a line: ");

		u64 count = 0;

		while (uart_recv() != '\n') {
			++count;
		}

		uart_send_str("The line has ");
		{
			char buf[21];
			u64_to_str(buf, count);
			uart_send_str(buf);
		}
		uart_send_str(" bytes.\r\n");
	}
}
