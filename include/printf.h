// Currently this is a bespoke, non-standard implementation.
//
// The available specifiers are:
// - `s` string
// - `c` character
// - `u` u64
// - `d` i64
// - `f` f64
// - `x` u64/usize/pointer as hex
// - `b` u8 as hex
// - `B` boolean
// - `D` "data", binary data as hex. Consumes two arguments: `u8 const* data` and `usize length`.
// - `%` literal percent sign
//
// XXX become standard. Probably will do it in Rust if possible since parsing in C is a nightmare.

#pragma once

typedef void (*printf_write_callback_t)(void* user, char ch);

void dprintf(printf_write_callback_t const write, void* user, char const* const fmt, ...);
void vdprintf(printf_write_callback_t const write, void* user, char const* const fmt, __builtin_va_list args);
