#include "string.h"

static char const DIGITS[16] = "0123456789abcdef";

void u64_to_str_hex(char buf[16], u64 value) {
	for (int i = 0; i < 16; ++i) {
		u64 const this_digit = value >> (64 - 4);
		buf[i] = DIGITS[this_digit];
		value <<= 4;
	}
}

void u8_to_str_hex(char buf[2], u8 const value) {
	buf[0] = DIGITS[value >> 4];
	buf[1] = DIGITS[value & 0xf];
}
