// There are four timers.
// Timers 0 and 2 are used by other hardware, so timers 1 and 3 are available to software.
// Timer 1 is used by the sleep code.

#pragma once

#include "base.h"

void timer_acknowledge(u8 timer);
u64 timer_get_micros(void);
// Only takes the lower 32 bits so multiple cycles may be necessary to sleep for long periods of time.
void timer_set_compare(u8 timer, u32 end_micros);
