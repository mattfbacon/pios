#include "timer.h"

static struct {
	u32 control_status;
	u32 counter_lo;
	u32 counter_hi;
	u32 compare[4];
} volatile* const TIMER_BASE = (void volatile*)(PERIPHERAL_BASE + 0x3000);

void timer_acknowledge(u8 const timer) {
	TIMER_BASE->control_status |= 1 << timer;
}

u64 timer_get_micros(void) {
	u32 hi = TIMER_BASE->counter_hi;
	u32 lo = TIMER_BASE->counter_lo;
	// Check if `lo` overflowed causing `hi` to increment.
	if (TIMER_BASE->counter_hi != hi) {
		hi = TIMER_BASE->counter_hi;
		lo = TIMER_BASE->counter_lo;
	}
	return (u64)hi << 32 | (u64)lo;
}

void timer_set_compare(u8 const timer, u32 const end_micros) {
	TIMER_BASE->compare[timer] = end_micros;
}
