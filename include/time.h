#pragma once

typedef i64 time_t;

struct time_components {
	// 0 = the year 1900.
	i32 year;
	// 0-indexed.
	u16 day_of_year;
	u8 second;
	u8 minute;
	u8 hour;
	// 1-indexed.
	u8 day_of_month;
	// 0-indexed.
	u8 month;
	// 0 = Sunday.
	u8 weekday;
};

bool time_unix_to_components(time_t time, struct time_components* ret);
time_t time_unix_from_components(struct time_components const* components);
