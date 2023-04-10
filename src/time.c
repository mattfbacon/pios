// These implementations are based on musl's.
// time_unix_to_components is similar to gmtime_r.
// time_unix_from_components is similar to mktime.

#include "time.h"

enum : i64 {
	LEAPS_PER_4 = 1,
	LEAPS_PER_100 = 24,
	LEAPS_PER_400 = 97,

	DAYS_PER_4_YEARS = DAYS_PER_YEAR * 4 + LEAPS_PER_4,
	DAYS_PER_100_YEARS = DAYS_PER_YEAR * 100 + LEAPS_PER_100,
	DAYS_PER_400_YEARS = DAYS_PER_YEAR * 400 + LEAPS_PER_400,

	EARLY_FACTOR = (DAYS_PER_YEAR + 1) * SECONDS_PER_DAY,
	EARLY_MINIMUM = I32_MIN * (i64)EARLY_FACTOR,
	EARLY_MAXIMUM = I32_MAX * (i64)EARLY_FACTOR,

	// 2000-03-01 00:00:00.
	LEAP_EPOCH = 946684800LL + SECONDS_PER_DAY * (31 + 29),
};

// Matching with `LEAP_EPOCH`, starts with March.
static u8 month_days[MONTHS_PER_YEAR] = { 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 31, 29 };

static time_t year_to_secs(i64 const year, bool* const is_leap) {
	// The years 1902..=2038.
	if (year >= 2 && year <= 138) {
		i32 const year_1968 = (i32)(year - 68);

		i32 leaps = (year_1968 / 4) * LEAPS_PER_4;
		*is_leap = year_1968 % 4 == 0;
		if (*is_leap) {
			--leaps;
		}

		return SECONDS_PER_YEAR * (year - 70) + SECONDS_PER_DAY * leaps;
	}

	i64 const year_2000 = year - 100;

	i32 cycles_400 = (i32)(year_2000 / 400);
	i32 within_400 = year_2000 % 400;

	if (within_400 < 0) {
		--cycles_400;
		within_400 += 400;
	}

	if (within_400 == 0) {
		*is_leap = true;
	}

	i32 const cycles_100 = within_400 / 100;
	i32 const within_100 = within_400 % 100;

	i32 const cycles_4 = within_100 / 4;
	i32 const within_4 = within_100 % 4;

	*is_leap = within_400 == 0 || (within_4 == 0 && within_100 != 0);

	i32 const leaps = LEAPS_PER_400 * cycles_400 + LEAPS_PER_100 * cycles_100 + LEAPS_PER_4 * cycles_4 - *is_leap;

	return (year_2000 * DAYS_PER_YEAR + leaps + 10958) * SECONDS_PER_DAY;
}

bool time_is_leap_year(i32 const year) {
	return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

u16 time_month_to_day_of_year(u8 const month, bool const is_leap_year) {
	// Prefix sums of days based on month.
	// Each entry is for the 1st of that month.
	// February has 28 days, i.e., it is not a leap year.
	static const u16 lut[MONTHS_PER_YEAR] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

	u16 days = lut[month];

	// Add the 29th day to February if necessary (month 1 is February since we are zero-indexed).
	if (is_leap_year && month > 1) {
		days += 1;
	}

	return days;
}

static time_t month_to_secs(u8 const month, bool const is_leap) {
	return time_month_to_day_of_year(month, is_leap) * SECONDS_PER_DAY;
}

bool time_unix_to_components(time_t const time, struct time_components* const ret) {
	if (time < EARLY_MINIMUM || time > EARLY_MAXIMUM) {
		return false;
	}

	i64 const seconds = time - LEAP_EPOCH;
	i64 days = seconds / SECONDS_PER_DAY;
	i32 seconds_within_day = seconds % SECONDS_PER_DAY;
	if (seconds_within_day < 0) {
		seconds_within_day += SECONDS_PER_DAY;
		days -= 1;
	}

	// 2000-03-01, our "epoch", is a Wednesday. In the `weekday` field, 3 is Wednesday.
	i32 weekday = (days + 3) % DAYS_PER_WEEK;
	if (weekday < 0) {
		weekday += DAYS_PER_WEEK;
	}

	i32 cycles_400_years = (i32)(days / DAYS_PER_400_YEARS);
	i32 days_within = days % DAYS_PER_400_YEARS;
	if (days_within < 0) {
		days_within += DAYS_PER_400_YEARS;
		cycles_400_years -= 1;
	}

	i32 cycles_100_years = days_within / DAYS_PER_100_YEARS;
	if (cycles_100_years == 4) {
		cycles_100_years -= 1;
	}
	days_within -= cycles_100_years * DAYS_PER_100_YEARS;

	i32 cycles_4_years = days_within / DAYS_PER_4_YEARS;
	if (cycles_4_years == 25) {
		cycles_4_years -= 1;
	}
	days_within -= cycles_4_years * DAYS_PER_4_YEARS;

	i32 remaining_years = days_within / DAYS_PER_YEAR;
	if (remaining_years == 4) {
		remaining_years -= 1;
	}
	days_within -= remaining_years * DAYS_PER_YEAR;

	bool const leap = remaining_years == 0 && (cycles_4_years != 0 || cycles_100_years == 0);
	i32 day_of_year = days_within + 31 + 28 + (i32)leap;
	i32 const year_length = 365 + (i32)leap;
	if (day_of_year >= year_length) {
		day_of_year -= year_length;
	}

	i32 years = remaining_years + 4 * cycles_4_years + 100 * cycles_100_years + 400 * cycles_400_years;

	i32 month = 0;
	for (; month_days[month] <= days_within; ++month) {
		days_within -= month_days[month];
	}

	if (years + 100 > I32_MAX || years + 100 < I32_MIN) {
		return false;
	}

	// From relative to March to relative to January.
	month += 2;
	if (month >= MONTHS_PER_YEAR) {
		month -= MONTHS_PER_YEAR;
		++years;
	}

	ret->year = years + 2000;
	ret->month = (u8)month;
	ret->day_of_month = (u8)days_within + 1;
	ret->weekday = (u8)weekday;
	ret->day_of_year = (u16)day_of_year;
	ret->hour = (u8)(seconds_within_day / SECONDS_PER_HOUR);
	ret->minute = (u8)(seconds_within_day / SECONDS_PER_MINUTE % MINUTES_PER_HOUR);
	ret->second = (u8)(seconds_within_day % SECONDS_PER_MINUTE);

	return true;
}

time_t time_unix_from_components(struct time_components const* const components) {
	i64 year = (i64)components->year - 1900;
	u8 month = components->month;

	// We need to do this check with the month because it affects leap-year checking.
	// For all other fields it would have no effect on the output.
	if (month >= MONTHS_PER_YEAR) {
		year += month / MONTHS_PER_YEAR;
		month %= MONTHS_PER_YEAR;
	}

	bool is_leap;
	time_t ret = year_to_secs(year, &is_leap);
	ret += month_to_secs(month, is_leap);
	ret += SECONDS_PER_DAY * (components->day_of_month - 1);
	ret += SECONDS_PER_HOUR * components->hour;
	ret += SECONDS_PER_MINUTE * components->minute;
	ret += components->second;

	return ret;
}
