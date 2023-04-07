#include "math.h"

f32 pow_f32_u32(f32 base, u32 const exponent) {
	f32 ret = 1.0f;

	for (u32 i = 0; i < exponent; ++i) {
		ret *= base;
	}

	return ret;
}

f64 pow_f64_u32(f64 base, u32 const exponent) {
	f64 ret = 1.0;

	for (u32 i = 0; i < exponent; ++i) {
		ret *= base;
	}

	return ret;
}
