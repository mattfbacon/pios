// In addition to implementing the standard printf interface, our implementation provides these custom conversion specifiers:
// - `%@d`: takes `u8 const* data, usize length` and prints hex bytes.
// - `%@b`: takes `bool` and prints `t` or `f` in normal mode and `true` and `false` in alternate mode.
#pragma once

typedef void (*printf_write_callback_t)(void* user, char ch);

// Returns the number of characters printed, or a negative value for errors.
isize dprintf(printf_write_callback_t const write, void* user, char const* const fmt, ...);
// Returns the number of characters printed, or a negative value for errors.
isize vdprintf(printf_write_callback_t const write, void* user, char const* const fmt, __builtin_va_list args);
