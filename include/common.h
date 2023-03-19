#pragma once

typedef unsigned char u8;
typedef signed char i8;
typedef unsigned short u16;
typedef signed short i16;
typedef unsigned int u32;
typedef signed int i32;
typedef unsigned long long u64;
typedef signed long long i64;
typedef float f32;
typedef double f64;

typedef u64 usize;
typedef i64 isize;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wenum-too-large"  // Not an enum.
enum {
	U8_MIN = 0,
	U8_MAX = 255,

	U16_MIN = 0,
	U16_MAX = 65536,

	U32_MIN = 0U,
	U32_MAX = 4294967295U,

	U64_MIN = 0ULL,
	U64_MAX = 18446744073709551615ULL,

	I8_MAX = 127,
	I8_MIN = -I8_MAX - 1,

	I16_MAX = 65535,
	I16_MIN = -I16_MAX - 1,

	I32_MAX = 2147483647,
	I32_MIN = -I32_MAX - 1,

	I64_MAX = 9223372036854775807LL,
	I64_MIN = -I64_MAX - 1,
};
#pragma GCC diagnostic pop

#define NULL ((void*)0)

#define offsetof __builtin_offsetof
#define alignof __alignof__
