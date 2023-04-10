#pragma once

#include "time.h"

enum : time_t {
	DS3231_YEAR_MIN = 1900,
	DS3231_YEAR_MAX = 2099,
};

bool ds3231_get_time(struct time_components* ret);
bool ds3231_set_time(struct time_components const* time);
