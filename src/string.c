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

char char_to_upper(char const ch) {
	if (ch >= 'a' && ch <= 'z') {
		return ch + 'A' - 'a';
	} else {
		return ch;
	}
}

char char_to_lower(char const ch) {
	if (ch >= 'A' && ch <= 'Z') {
		return ch + 'a' - 'A';
	} else {
		return ch;
	}
}

void str_make_uppercase(char* str) {
	for (; *str != '\0'; ++str) {
		*str = char_to_upper(*str);
	}
}

void str_make_lowercase(char* str) {
	for (; *str != '\0'; ++str) {
		*str = char_to_lower(*str);
	}
}
