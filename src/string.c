#include "string.h"

void memcpy(char* const dest, char const* const src, usize const len) {
	for (usize i = 0; i < len; ++i) {
		dest[i] = src[i];
	}
}

static char const DIGITS[16] = "0123456789abcdef";

void u64_to_str(char buf[21], u64 value) {
	if (value == 0) {
		buf[0] = '0';
		buf[1] = '\0';
		return;
	}

	char internal[20];
	usize internal_idx = 20;

	while (value != 0) {
		u64 const this_digit = value % 10;
		--internal_idx;
		internal[internal_idx] = DIGITS[this_digit];
		value /= 10;
	}

	usize const len = 20 - internal_idx;
	memcpy(buf, internal + internal_idx, len);
	buf[len] = '\0';
}

void u64_to_str_hex(char buf[16], u64 value) {
	for (int i = 0; i < 16; ++i) {
		u64 const this_digit = value >> (64 - 4);
		buf[i] = DIGITS[this_digit];
		value <<= 4;
	}
}
