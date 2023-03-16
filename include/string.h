#pragma once

inline void memset(void* const data_, u8 const fill, usize const length) {
	char* const data = data_;
	for (usize i = 0; i < length; ++i) {
		data[i] = fill;
	}
}

inline void memcpy(void* const out_, void const* const in_, usize const len) {
	char* const out = out_;
	char const* const in = in_;
	for (usize i = 0; i < len; ++i) {
		out[i] = in[i];
	}
}

inline void memcpy_volatile(void volatile* const out_, void const volatile* const in_, usize const len) {
	char volatile* const out = out_;
	char const volatile* const in = in_;
	for (usize i = 0; i < len; ++i) {
		out[i] = in[i];
	}
}

inline void memmove(void* const dest_, void const* const src_, usize const length) {
	if (dest_ < src_) {
		// Moving characters backward, so copy forward.
		memcpy(dest_, src_, length);
	} else if (dest_ > src_) {
		// Moving characters forward, so copy backward.
		char* const dest = dest_;
		char const* const src = src_;
		for (usize i = length; i > 0; --i) {
			dest[i - 1] = src[i - 1];
		}
	} else {
		// Not moving at all, so do nothing.
	}
}

inline int strcmp(char const* s1, char const* s2) {
	while (*s1 != '\0' && *s1 == *s2) {
		++s1;
		++s2;
	}
	return *s1 - *s2;
}

// does add a NUL terminator as the string's length may vary.
void u64_to_str(char buf[21], u64 value);
// does add a NUL terminator as the string's length may vary.
void i64_to_str(char buf[22], u64 value);
// does add a NUL terminator as the string's length may vary.
void f64_to_str(char buf[25], f64 value);
// does not add a NUL terminator as the string will always be 16 characters long.
void u64_to_str_hex(char buf[16], u64 value);
// does not add a NUL terminator as the string will always be 2 characters long.
void u8_to_str_hex(char buf[2], u8 value);
