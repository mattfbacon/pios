#pragma once

typedef void (*printf_write_callback_t)(void* user, char ch);

// Returns the number of characters printed.
usize dprintf(printf_write_callback_t const write, void* user, char const* const fmt, ...);
// Returns the number of characters printed.
usize vdprintf(printf_write_callback_t const write, void* user, char const* const fmt, __builtin_va_list args);
