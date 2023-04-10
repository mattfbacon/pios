// # Resources
//
// - <https://www.analog.com/media/en/technical-documentation/data-sheets/DS3231.pdf>

#include "devices/ds3231.h"
#include "i2c.h"
#include "sleep.h"
#include "try.h"

enum : i2c_address_t {
	ADDRESS = 0x68,
};

enum : u8 {
	REG_DATETIME = 0,

	FLAG_PM = 1 << 5,
	FLAG_12_HOURS = 1 << 6,
	MASK_HOUR_12 = 0b1'1111,
	MASK_HOUR_24 = 0b11'1111,

	MASK_WEEKDAY = 0b111,

	MASK_MONTH = 0b1'1111,
	SHIFT_CENTURY = 7,
};

static u8 bcd_encode(u8 const normal) {
	// bcd = 16 * (normal / 10) + (normal % 10) = 10 * (normal / 10) + 6 * (normal / 10) + (normal % 10)
	// normal = 10 * (normal / 10) + (normal % 10)
	// bcd = normal + 6 * (normal / 10)
	return normal + 6 * (normal / 10);
}

static u8 bcd_decode(u8 const bcd) {
	// normal = 10 * (bcd / 16) + (bcd % 16) = 16 * (bcd / 16) - 6 * (bcd / 16) + (bcd % 16)
	// bcd = 16 * (bcd / 16) + (bcd % 16)
	// normal = bcd - 6 * (bcd / 16)
	return bcd - 6 * (bcd / 16);
}

bool ds3231_get_time(struct time_components* const ret) {
	u8 buf[8] = {
		REG_DATETIME,
	};

	TRY(i2c_send(ADDRESS, buf, 1))
	// Delay determined from testing.
	sleep_micros(100);
	TRY(i2c_recv(ADDRESS, buf, sizeof(buf)))

	ret->second = bcd_decode(buf[0]);

	ret->minute = bcd_decode(buf[1]);

	bool const has_12_hours = buf[2] & FLAG_12_HOURS;
	if (has_12_hours) {
		bool const pm = buf[2] & FLAG_PM;
		ret->hour = bcd_decode(buf[2] & MASK_HOUR_12) + (pm ? 12 : 0);
	} else {
		ret->hour = bcd_decode(buf[2] & MASK_HOUR_24);
	}

	ret->weekday = (buf[3] & MASK_WEEKDAY) - 1;

	ret->day_of_month = bcd_decode(buf[4]);

	ret->month = bcd_decode(buf[5] & MASK_MONTH);

	u8 const century = buf[5] >> SHIFT_CENTURY;
	u8 const within_century = bcd_decode(buf[6]);
	ret->year = 100 * century + within_century + 1900;

	// The DS3231 does not provide this so we need to synthesize it.
	ret->day_of_year = time_month_to_day_of_year(ret->month, time_is_leap_year(ret->year)) + ret->day_of_month;

	return true;
}

bool ds3231_set_time(struct time_components const* const time) {
	TRY_MSG(time->year >= DS3231_YEAR_MIN && time->year <= DS3231_YEAR_MAX)

	u8 const buf[8] = {
		REG_DATETIME,
		bcd_encode(time->second),
		bcd_encode(time->minute),
		// With bit 6 unset: 24 hour operation.
		bcd_encode(time->hour),
		time->weekday + 1,
		bcd_encode(time->day_of_month),
		bcd_encode(time->month + 1) | (time->year >= 2000 ? (1 << 7) : 0),
		bcd_encode((u8)(time->year % 100)),
	};

	TRY(i2c_send(ADDRESS, buf, sizeof(buf)))
	// Delay determined from testing.
	sleep_micros(1'000);

	return true;
}
