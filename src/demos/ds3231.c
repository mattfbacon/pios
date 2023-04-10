#include "devices/ds3231.h"
#include "i2c.h"
#include "sleep.h"
#include "time.h"
#include "try.h"
#include "uart.h"

static char const* days_of_week[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

static u64 read_u64(void) {
	u64 ret = 0;
	char ch;
	while ((ch = uart_recv()) != '\r') {
		ret *= 10;
		ret += ch - '0';
	}
	return ret;
}

[[noreturn]] void main(void) {
	struct time_components components;

	i2c_init(i2c_speed_standard);

	uart_send_str("unix timestamp to set to (0 to skip setting): ");
	time_t const timestamp = (i64)read_u64();

	if (timestamp != 0) {
		assert(time_unix_to_components(timestamp, &components), "converting timestamp to components");

		uart_printf(
			"decomposed: %u-%u-%u %u:%u:%u\r\n",
			(u64)(components.year),
			(u64)components.month + 1,
			(u64)components.day_of_month,
			(u64)components.hour,
			(u64)components.minute,
			(u64)components.second);

		assert(ds3231_set_time(&components), "setting time");
	}

	while (true) {
		assert(ds3231_get_time(&components));

		uart_printf(
			"%s %u-%u-%u %u:%u:%u\r\n",
			days_of_week[components.weekday],
			components.year,
			components.month,
			components.day_of_month,
			components.hour,
			components.minute,
			components.second);

		sleep_micros(1'000'000);
	}
}
