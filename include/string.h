#pragma once

inline void memcpy(char* const out, char const* const in, usize const len) {
	for (usize i = 0; i < len; ++i) {
		out[i] = in[i];
	}
}

inline void memcpy_volatile(char volatile* const out, char const volatile* const in, usize const len) {
	for (usize i = 0; i < len; ++i) {
		out[i] = in[i];
	}
}

// does add a NUL terminator as the string's length may vary.
void u64_to_str(char buf[21], u64 value);
// does add a NUL terminator as the string's length may vary.
void i64_to_str(char buf[22], u64 value);
// does not add a NUL terminator as the string will always be 16 characters long.
void u64_to_str_hex(char buf[16], u64 value);
