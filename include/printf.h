#pragma once

typedef void (*printf_write_callback_t)(void* user, char ch);

void dprintf(printf_write_callback_t const write, void* user, char const* const fmt, ...);
void vdprintf(printf_write_callback_t const write, void* user, char const* const fmt, __builtin_va_list args);
