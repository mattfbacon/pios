// # References
//
// - <https://github.com/adafruit/Adafruit_AHTX0>
// - <https://cdn-learn.adafruit.com/assets/assets/000/091/676/original/AHT20-datasheet-2020-4-16.pdf>

#include "devices/aht20.h"
#include "halt.h"
#include "i2c.h"
#include "init.h"
#include "sleep.h"
#include "string.h"
#include "try.h"
#include "uart.h"

enum {
	SENSOR_ADDRESS = 0x38,

	COMMAND_GET_STATUS = 0x71,
	COMMAND_CALIBRATE = 0xe1,
	COMMAND_MEASURE = 0xac,
	COMMAND_SOFT_RESET = 0xba,

	STATUS_BUSY = 0x80,
	STATUS_CALIBRATED = 0x08,

	STATUS_INITIAL_EXPECTED_MASK = 0x18,
};

static bool send(u8 const* const buf, u32 const len) {
	return i2c_send(SENSOR_ADDRESS, buf, len);
}

static bool recv(u8* const buf, u32 const len) {
	return i2c_recv(SENSOR_ADDRESS, buf, len);
}

static u8 get_status(void) {
	u8 ret;
	assert(recv(&ret, 1), "receive status");
	return ret;
}

static void wait_until_not_busy(void) {
	while (get_status() & STATUS_BUSY) {
		sleep_micros(10'000);
	}
}

// See section 8 of the datasheet.
static aht20_data_t decode_sensor_data(u8 const data[6]) {
	static f32 const humidity_factor = 9.5367431640625e-05f;
	static f32 const temperature_factor = 0.00019073486328125f;

	aht20_data_t ret;

	u32 const humidity_raw = (data[1] << 12) | (data[2] << 4) | (data[3] >> 4);
	ret.humidity = (f32)humidity_raw * humidity_factor;

	u32 const temperature_raw = ((data[3] & 0xf) << 16) | (data[4] << 8) | data[5];
	ret.temperature = (f32)temperature_raw * temperature_factor - 50.0f;

	return ret;
}

// See section 7.4 of the datasheet.
void aht20_init() {
	LOG_DEBUG("initializing AHT20 sensor");

	u8 buf = COMMAND_GET_STATUS;
	assert(send(&buf, 1), "send status command");
	// Apparently we're supposed to initialize some registers if this isn't true.
	// We haven't implemented that, and it's never been an issue.
	assert((get_status() & STATUS_INITIAL_EXPECTED_MASK) == STATUS_INITIAL_EXPECTED_MASK, "initial status is incorrect");
}

// See section 7.4 of the datasheet.
aht20_data_t aht20_measure() {
	LOG_DEBUG("measuring from sensor");

	u8 buf[7];

	buf[0] = COMMAND_MEASURE;
	buf[1] = 0x33;
	buf[2] = 0x00;
	assert(send(buf, 3), "send measure command");

	sleep_micros(80'000);
	wait_until_not_busy();

	assert(recv(buf, 7), "receive measurement data");

	aht20_data_t const data = decode_sensor_data(buf);
	LOG_DEBUG("sensor data: humidity %f, temperature %f", data.humidity, data.temperature);
	return data;
}
