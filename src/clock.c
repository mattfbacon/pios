#include "clock.h"

static u32 volatile* const CLOCK_BASE = (u32 volatile*)0xfe101000;

enum {
	CLOCK_CONTROL = 0,
	CLOCK_DIVISOR = 1,
	CLOCK_PASSWORD = 0x5a000000,
	CLOCK_BUSY = 1 << 7,
	CLOCK_DISABLE = 1 << 5,
	CLOCK_ENABLE = 1 << 4,
	CLOCK_SOURCE_OSCILLATOR = 1,
};

void clock_init(clock_id_t const clock, u32 const divisor) {
	u32 volatile* reg = &CLOCK_BASE[(u32)clock];

	// clock must be disabled before its divisor can be changed
	reg[CLOCK_CONTROL] = CLOCK_PASSWORD | CLOCK_DISABLE;
	while (reg[CLOCK_CONTROL] & CLOCK_BUSY)
		;

	reg[CLOCK_DIVISOR] = CLOCK_PASSWORD | (divisor << 12);

	// datasheet says not to enable clock at the same time as setting the source
	reg[CLOCK_CONTROL] = CLOCK_PASSWORD | CLOCK_SOURCE_OSCILLATOR;
	reg[CLOCK_CONTROL] = CLOCK_PASSWORD | CLOCK_SOURCE_OSCILLATOR | CLOCK_ENABLE;
}
