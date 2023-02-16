#include "printf.h"
#include "string.h"

static void write_str(void (*const write)(char), char const* str) {
	for (; *str; ++str) {
		write(*str);
	}
}

void vdprintf(void (*const write)(char), char const* fmt, __builtin_va_list args) {
	for (; *fmt; ++fmt) {
		char const ch = *fmt;

		if (ch != '%') {
			write(ch);
			continue;
		}

		++fmt;
		char const specifier = *fmt;
		switch (specifier) {
			case '\0': {
				write_str(write, "ERROR incomplete format specifier\r\n");
				return;
			}
			case 's': {
				char const* const arg = __builtin_va_arg(args, char const*);
				write_str(write, arg);
				break;
			}
			case 'c': {
				char const arg = (char)__builtin_va_arg(args, u32);
				write(arg);
				break;
			}
			case 'u': {
				u64 const arg = __builtin_va_arg(args, u64);
				char itoa_buf[21];
				u64_to_str(itoa_buf, arg);
				write_str(write, itoa_buf);
				break;
			}
			case 'x': {
				u64 const arg = __builtin_va_arg(args, u64);
				char itoa_buf[17];
				u64_to_str_hex(itoa_buf, arg);
				itoa_buf[16] = '\0';
				write_str(write, itoa_buf);
				break;
			}
			case 'b': {
				u64 const arg = __builtin_va_arg(args, u64);
				char itoa_buf[17];
				u64_to_str_hex(itoa_buf, arg);
				itoa_buf[16] = '\0';
				write_str(write, itoa_buf + 14);
				break;
			}
			case 'B': {
				bool const arg = (bool)__builtin_va_arg(args, u32);
				write_str(write, arg ? "true" : "false");
				break;
			}
			case '%': {
				write('%');
				break;
			}
			default: {
				write_str(write, "ERROR unknown format specifier %");
				write(specifier);
				write_str(write, "\r\n");
				return;
			}
		}
	}
}
