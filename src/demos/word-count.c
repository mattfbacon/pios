#include "gpio.h"
#include "string.h"
#include "uart.h"

[[noreturn]] void main(void) {
	while (true) {
		uart_send_str("Enter a line: ");

		usize count = 0;

		while (uart_recv() != '\r') {
			++count;
		}

		uart_printf("The line has %zu bytes\r\n", count);
	}
}
