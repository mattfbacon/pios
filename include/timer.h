#pragma once

#include "base.h"

enum {
	TIMER_FREQ = 1'000'000,
};

static struct {
	u32 control_status;
	u32 counter_lo;
	u32 counter_hi;
	u32 compare[4];
} volatile* const TIMER_BASE = (void volatile*)(PERIPHERAL_BASE + 0x3000);

u64 timer_get_count(void);
