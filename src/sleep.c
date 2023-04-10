#include "base.h"
#include "exception.h"
#include "sleep.h"
#include "timer.h"

enum : u8 {
	SLEEP_TIMER = 1,
};

void sleep_cycles(u64 const cycles) {
	for (u64 i = 0; i < cycles; ++i) {
		asm volatile("isb");
	}
}

static void sleep_micros_spin(u64 const end) {
	while (timer_get_micros() < end) {
		asm volatile("isb");
	}
}

static void sleep_micros_interrupts(u64 const end) {
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
		sleep_micros_interrupts(timer_get_micros() + micros);
	} else {
		sleep_micros_spin(timer_get_micros() + micros);
	}
}

void sleep_until(u64 const end) {
	if (timer_get_micros() >= end) {
		return;
	}
	if (!(exception_get_mask() & exception_mask_irq)) {
		sleep_micros_interrupts(end);
	} else {
		sleep_micros_spin(end);
	}
}
