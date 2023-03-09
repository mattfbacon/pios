#include "log.h"
#include "uart.h"

void log_write(char const* file, u32 line, char const* level, char const* fmt, ...) {
	uart_printf("[%s %s:%u] ", level, file, line);

	__builtin_va_list args;
	__builtin_va_start(args, fmt);
	uart_vprintf(fmt, args);
	__builtin_va_end(args);

	uart_send_str("\r\n");
}
