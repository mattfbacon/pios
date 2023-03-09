#include "printf.h"
#include "string.h"

static void write_str(printf_write_callback_t const write, void* const user, char const* str) {
	for (; *str; ++str) {
		write(user, *str);
	}
}

static void write_bytes_hex(printf_write_callback_t const write, void* const user, u8 const* const buf, usize const len) {
	char itoa_buf[3];
	itoa_buf[2] = '\0';
	for (usize i = 0; i < len; ++i) {
		u8_to_str_hex(itoa_buf, buf[i]);
		write_str(write, user, itoa_buf);
	}
}

void dprintf(printf_write_callback_t const write, void* user, char const* fmt, ...) {
	__builtin_va_list args;
	__builtin_va_start(args, fmt);
	vdprintf(write, user, fmt, args);
	__builtin_va_end(args);
}

void vdprintf(printf_write_callback_t const write, void* const user, char const* fmt, __builtin_va_list args) {
	for (; *fmt; ++fmt) {
		char const ch = *fmt;

		if (ch != '%') {
			write(user, ch);
			continue;
		}

		++fmt;
		char const specifier = *fmt;
		switch (specifier) {
			case '\0': {
				write_str(write, user, "ERROR incomplete format specifier\r\n");
				return;
			}
			case 's': {
				char const* const arg = __builtin_va_arg(args, char const*);
				write_str(write, user, arg);
				break;
			}
			case 'c': {
				char const arg = (char)__builtin_va_arg(args, u32);
				write(user, arg);
				break;
			}
			case 'u': {
				u64 const arg = __builtin_va_arg(args, u64);
				char itoa_buf[32];
				u64_to_str(itoa_buf, arg);
				write_str(write, user, itoa_buf);
				break;
			}
			case 'd': {
				i64 const arg = __builtin_va_arg(args, i64);
				char itoa_buf[32];
				i64_to_str(itoa_buf, arg);
				write_str(write, user, itoa_buf);
				break;
			}
			case 'x': {
				u64 const arg = __builtin_va_arg(args, u64);
				char itoa_buf[17];
				u64_to_str_hex(itoa_buf, arg);
				itoa_buf[16] = '\0';
				write_str(write, user, itoa_buf);
				break;
			}
			case 'b': {
				u8 const arg = (u8) __builtin_va_arg(args, u32);
				write_bytes_hex(write, user, &arg, 1);
				break;
			}
			case 'B': {
				bool const arg = (bool)__builtin_va_arg(args, u32);
				write_str(write, user, arg ? "true" : "false");
				break;
			}
			case 'D': {
				u8 const* const buf = __builtin_va_arg(args, u8 const*);
				usize const len = __builtin_va_arg(args, usize);
				write_bytes_hex(write, user, buf, len);
				break;
			}
			case '%': {
				write(user, '%');
				break;
			}
			default: {
				write_str(write, user, "ERROR unknown format specifier %");
				write(user, specifier);
				write_str(write, user, "\r\n");
				return;
			}
		}
	}
}
