#include "sleep.h"

void sleep_cycles(u64 cycles) {
	for (; cycles > 0; --cycles) {
		asm volatile("isb");
	}
}

static u32 counter_timer_frequency(void) {
	u32 ret;
	asm volatile("mrs %0, cntfrq_el0" : "=r"(ret));
	return ret;
}

static u64 counter_timer_count(void) {
	u64 ret;
	asm volatile("mrs %0, cntpct_el0" : "=r"(ret));
	return ret;
}

void sleep_micros(u64 const micros) {
	u32 const counter_hz = counter_timer_frequency();
	u64 const steps_needed = ((u64)counter_hz * micros) / 1000000;
	u64 const counter_end = counter_timer_count() + steps_needed;

	while (counter_timer_count() < counter_end)
		;
}
