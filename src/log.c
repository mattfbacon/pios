#include "log.h"
#include "uart.h"

static char const* const LEVELS[] = { "trace", "debug", "info", "warn", "error", "fatal" };
static char const* const COLORS[] = { "0;37", "0", "1;30", "1;33", "1;31", "41m\e[1;97" };

void log_write(char const* const file, u32 const line, u32 const level, char const* const fmt, ...) {
	uart_printf("\e[%sm[%s %s:%u] ", COLORS[level], LEVELS[level], file, line);

	__builtin_va_list args;
	__builtin_va_start(args, fmt);
	uart_vprintf(fmt, args);
	__builtin_va_end(args);

	uart_send_str("\e[0m\r\n");
}
