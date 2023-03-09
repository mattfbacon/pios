#pragma once

#include "base.h"

void timer_acknowledge(u8 timer);
u64 timer_get_micros(void);
// Only takes the lower 32 bits so multiple cycles may be necessary to sleep for long periods of time.
void timer_set_compare(u32 end_micros);
