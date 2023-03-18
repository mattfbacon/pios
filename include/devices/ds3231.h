#pragma once

#include "time.h"

bool ds3231_get_time(struct time_components* ret);
bool ds3231_set_time(struct time_components const* time);
