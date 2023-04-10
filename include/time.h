#pragma once

typedef i64 time_t;

enum : time_t {
	SECONDS_PER_MINUTE = 60,
	MINUTES_PER_HOUR = 60,
	HOURS_PER_DAY = 24,
	DAYS_PER_WEEK = 7,
	DAYS_PER_YEAR = 365,
	MONTHS_PER_YEAR = 12,
	SECONDS_PER_HOUR = SECONDS_PER_MINUTE * MINUTES_PER_HOUR,
	SECONDS_PER_DAY = SECONDS_PER_HOUR * HOURS_PER_DAY,
	SECONDS_PER_YEAR = SECONDS_PER_DAY * DAYS_PER_YEAR,
};

struct time_components {
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

u16 time_month_to_day_of_year(u8 month, bool is_leap_year);
bool time_is_leap_year(i32 year);
