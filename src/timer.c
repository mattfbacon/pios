#include "timer.h"

u64 timer_get_count(void) {
	u32 hi = TIMER_BASE->counter_hi;
	u32 lo = TIMER_BASE->counter_lo;
	// check if `lo` overflowed causing `hi` to increment
	if (TIMER_BASE->counter_hi != hi) {
		hi = TIMER_BASE->counter_hi;
		lo = TIMER_BASE->counter_lo;
	}
	return (u64)hi << 32 | (u64)lo;
}
