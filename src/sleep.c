#include "base.h"
#include "sleep.h"
#include "timer.h"

void sleep_cycles(u64 cycles) {
	for (; cycles > 0; --cycles) {
		asm volatile("isb");
	}
}

static u32 counter_freq(void) {
	u64 ret;
	asm volatile("mrs %0, cntfrq_el0" : "=r"(ret));
	return (u32)ret;
}

static u64 counter_count(void) {
	u64 ret;
	asm volatile("mrs %0, cntpct_el0" : "=r"(ret));
	return ret;
}

static void sleep_micros_spin(u64 const micros) {
	u32 const counter_hz = counter_freq();
	u64 const steps_needed = ((u64)counter_hz * micros) / 1'000'000;
	u64 const counter_end = counter_count() + steps_needed;

	while (counter_count() < counter_end)
		;
}

static void sleep_micros_interrupts(u64 const micros) {
	u64 const end = timer_get_micros() + micros;

	// this truncates, which may cause more interrupts than necessary, but will not break the function, because we check for `get_counter() < end`.
	timer_set_compare((u32)end);
	while (timer_get_micros() < end) {
		asm volatile("wfe");
	}
}

static bool interrupts_enabled(void) {
	u64 daif;
	asm("mrs %0, daif" : "=r"(daif));
	return daif & (1 << 1);
}

void sleep_micros(u64 const micros) {
	if (interrupts_enabled() && micros >= SLEEP_MIN_MICROS_FOR_INTERRUPTS) {
		sleep_micros_interrupts(micros);
	} else {
		sleep_micros_spin(micros);
	}
}
