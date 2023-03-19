#include "base.h"
#include "exception.h"
#include "sleep.h"
#include "timer.h"

enum {
	SLEEP_TIMER = 1,
};

void sleep_cycles(u64 const cycles) {
	for (u64 i = 0; i < cycles; ++i) {
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
	// Multiply before dividing to avoid precision loss.
	u64 const steps_needed = ((u64)counter_hz * micros) / 1'000'000;
	u64 const counter_end = counter_count() + steps_needed;

	while (counter_count() < counter_end)
		;
}

static void sleep_micros_interrupts(u64 const micros) {
	u64 const end = timer_get_micros() + micros;

	// This truncates, which may cause more interrupts than necessary, but will not break the function, because we check for `get_counter() < end`.
	timer_set_compare(SLEEP_TIMER, (u32)end);
	while (timer_get_micros() < end) {
		asm volatile("wfe");
	}
}

void sleep_micros(u64 const micros) {
	// We'd like to use interrupts when possible.
	// Obviously if interrupts are disabled we can't use them.
	// They're also inaccurate if the sleep duration is too small.
	// In those cases we use a spinning technique instead.
	if (!(exception_get_mask() & exception_mask_irq) && micros >= SLEEP_MIN_MICROS_FOR_INTERRUPTS) {
		sleep_micros_interrupts(micros);
	} else {
		sleep_micros_spin(micros);
	}
}
