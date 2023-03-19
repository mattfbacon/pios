#include "devices/aht20.h"
#include "i2c.h"
#include "log.h"
#include "sleep.h"
#include "try.h"
#include "uart.h"

static void print_sensor_data(struct aht20_data const data) {
	LOG_INFO("the temperature is about %u °C and the humidity is about %u%%", (u32)data.temperature, (u32)data.humidity);
}

void main(void) {
	LOG_INFO("initializing i2c");
	i2c_init(i2c_speed_standard);

	// Wait for sensor to initialize.
	sleep_micros(100'000);

	LOG_INFO("initializing aht20");
	assert(aht20_init(), "initializing aht20");

	sleep_micros(10'000);

	while (true) {
		LOG_INFO("starting measuring");
		struct aht20_data data;
		assert(aht20_measure(&data), "measuring");
		print_sensor_data(data);
		sleep_micros(5'000'000);
	}
}
