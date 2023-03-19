#pragma once

enum {
	// The minimum possible sleep that will use interrupt-based, as opposed to spin-based, sleeping.
	SLEEP_MIN_MICROS_FOR_INTERRUPTS = 10,
};

void sleep_cycles(u64 cycles);
void sleep_micros(u64 micros);
