#include <stdint.h>

#include "sleep.h"

void sleep_cycles(unsigned long cycles) {
	for (; cycles > 0; --cycles) {
		asm volatile("isb");
	}
}

void sleep_micros(unsigned long const micros) {
	uint32_t counter_hz;
	asm volatile("mrs %0, cntfrq_el0" : "=r"(counter_hz));
	uint64_t counter_current;
	asm volatile("mrs %0, cntpct_el0" : "=r"(counter_current));

	// `(counter_hz / 1_000_000) * micros` but retains more precision
	uint64_t const counter_end = counter_current + (((uint64_t)counter_hz / 1000) * micros) / 1000;

	do {
		asm volatile("mrs %0, cntpct_el0" : "=r"(counter_current));
	} while (counter_current < counter_end);
}
