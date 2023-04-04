#pragma once

// Fill `length` bytes starting at `data_` with the repeated value `fill`.
inline void memset(void* const data_, u8 const fill, usize const length) {
	char* const data = data_;
	for (usize i = 0; i < length; ++i) {
		data[i] = fill;
	}
}

// Copy `length` bytes from `in_` to `out_`.
inline void memcpy(void* restrict const out_, void const* restrict const in_, usize const length) {
	char* const out = out_;
	char const* const in = in_;
	for (usize i = 0; i < length; ++i) {
		out[i] = in[i];
	}
}

// Copy `length` bytes from `in_` to `out_`, handling overlapping regions properly.
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

inline usize strlen(char const* str) {
	usize ret = 0;

	while (*str != '\0') {
		++ret;
		++str;
	}

	return ret;
}

inline usize strnlen(char const* str, usize max_length) {
	usize ret = 0;

	while (max_length > 0 && *str != '\0') {
		++ret;
		++str;
		--max_length;
	}

	return ret;
}

inline int strcmp(char const* restrict s1, char const* restrict s2) {
	while (*s1 != '\0' && *s1 == *s2) {
		++s1;
		++s2;
	}
	return *s1 - *s2;
}

// Does add a NUL terminator as the string's length may vary.
void u64_to_str(char buf[21], u64 value);
// Does add a NUL terminator as the string's length may vary.
void i64_to_str(char buf[22], u64 value);
// Does add a NUL terminator as the string's length may vary.
void f64_to_str(char buf[25], f64 value);
// Does not add a NUL terminator as the string will always be 16 characters long.
void u64_to_str_hex(char buf[16], u64 value);
// Does not add a NUL terminator as the string will always be 2 characters long.
void u8_to_str_hex(char buf[2], u8 value);
