#include "math.h"

f32 pow_u(f32 base, u32 const exponent) {
	f32 ret = 1.0f;

	for (u32 i = 0; i < exponent; ++i) {
		ret *= base;
	}

	return ret;
}
