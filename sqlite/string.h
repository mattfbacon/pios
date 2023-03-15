#pragma once

#include <stddef.h>

inline void* memset(void* const data_, int const fill, size_t const length) {
	char* const data = data_;
	for (size_t i = 0; i < length; ++i) {
		data[i] = (char)fill;
	}
	return data_;
}

inline int memcmp(void const* const s1_, void const* const s2_, size_t const length) {
	char const* s1 = (char const*)s1_;
	char const* s2 = (char const*)s2_;
	for (size_t i = 0; i < length; ++i) {
		if (*s1 != *s2) {
			return *s1 - *s2;
		}
		++s1;
		++s2;
	}
	return 0;
}

inline void* memcpy(void* const dest_, void const* const src_, size_t const length) {
	char* const dest = (char*)dest_;
	char const* const src = (char const*)src_;
	for (size_t i = 0; i < length; ++i) {
		dest[i] = src[i];
	}
	return dest_;
}

inline void* memmove(void* const dest_, void const* const src_, size_t const length) {
	if (dest_ < src_) {
		// Moving characters backward, so copy forward.
		memcpy(dest_, src_, length);
	} else if (dest_ > src_) {
		// Moving characters forward, so copy backward.
		char* const dest = (char*)dest_;
		char const* const src = (char const*)src_;
		for (size_t i = length; i > 0; --i) {
			dest[i - 1] = src[i - 1];
		}
	} else {
		// Not moving at all, so do nothing.
	}
	return dest_;
}

inline int strcmp(char const* s1, char const* s2) {
	while (*s1 != '\0' && *s1 == *s2) {
		++s1;
		++s2;
	}
	return *s1 - *s2;
}

inline int strncmp(char const* s1, char const* s2, size_t const size) {
	for (size_t i = 0; i < size; ++i) {
		if (*s1 == '\0' || *s1 != *s2) {
			return *s1 - *s2;
		}
		++s1;
		++s2;
	}
	return 0;
}

inline size_t strlen(char const* str) {
	size_t ret = 0;
	while (*str != '\0') {
		++ret;
		++str;
	}
	return ret;
}

inline char* strrchr(char const* str, int const ch) {
	char const* position = NULL;
	while (true) {
		if (*str == ch) {
			position = str;
		}
		if (*str == '\0') {
			break;
		}
		++str;
	}
	return (char*)position;
}

inline size_t strcspn(char const* str, char const* const reject) {
	size_t ret = 0;
	while (*str != '\0') {
		for (char const* reject_item = reject; *reject_item != '\0'; ++reject_item) {
			if (*str == *reject_item) {
				goto break_outer;
			}
		}
		++ret;
		++str;
	}
break_outer:;
	return ret;
}
