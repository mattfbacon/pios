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

#define U8_MIN ((u8)0)
#define U8_MAX ((u8)255)

#define U16_MIN ((u16)0)
#define U16_MAX ((u16)65536)

#define U32_MIN ((u32)0U)
#define U32_MAX ((u32)4294967295U)

#define U64_MIN ((u64)0ULL)
#define U64_MAX ((u64)18446744073709551615ULL)

#define I8_MAX ((i8)127)
#define I8_MIN ((i8)(-I8_MAX - (1)))

#define I16_MAX ((i16)65535)
#define I16_MIN ((i16)(-I16_MAX - (1)))

#define I32_MAX ((i32)2147483647)
#define I32_MIN ((i32)(-I32_MAX - (1)))

#define I64_MAX ((i64)9223372036854775807LL)
#define I64_MIN ((i64)(-I64_MAX - (1)))

#define USIZE_MAX ((usize)U64_MAX)
#define USIZE_MIN ((usize)U64_MIN)

#define ISIZE_MAX ((isize)I64_MAX)
#define ISIZE_MIN ((isize)I64_MIN)

#define NULL ((void*)0)

#define offsetof __builtin_offsetof
#define alignof __alignof__

#define asm __asm__
