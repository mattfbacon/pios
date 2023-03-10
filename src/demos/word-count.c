#include "gpio.h"
#include "string.h"
#include "uart.h"

void main(void) {
	while (true) {
		uart_send_str("Enter a line: ");

		u64 count = 0;

		while (uart_recv() != '\r') {
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
