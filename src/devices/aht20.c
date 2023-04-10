// # References
//
// - <https://github.com/adafruit/Adafruit_AHTX0>
// - <https://cdn-learn.adafruit.com/assets/assets/000/091/676/original/AHT20-datasheet-2020-4-16.pdf>

#include "devices/aht20.h"
#include "halt.h"
#include "i2c.h"
#include "sleep.h"
#include "string.h"
#include "try.h"
#include "uart.h"

enum : i2c_address_t {
	SENSOR_ADDRESS = 0x38,
};

enum : u8 {
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

static bool get_status(u8* const ret) {
	return recv(ret, 1);
}

static bool wait_until_not_busy(void) {
	u8 status;

	while (true) {
		TRY(get_status(&status))
		if (!(status & STATUS_BUSY)) {
			break;
		}
		sleep_micros(10'000);
	}

	return true;
}

// See section 8 of the datasheet.
static struct aht20_data decode_sensor_data(u8 const data[6]) {
	static f32 const humidity_factor = 9.5367431640625e-05f;
	static f32 const temperature_factor = 0.00019073486328125f;

	struct aht20_data ret;

	u32 const humidity_raw = ((u32)data[1] << 12) | ((u32)data[2] << 4) | ((u32)data[3] >> 4);
	ret.humidity = (f32)humidity_raw * humidity_factor;

	u32 const temperature_raw = (((u32)data[3] & 0xf) << 16) | ((u32)data[4] << 8) | (u32)data[5];
	ret.temperature = (f32)temperature_raw * temperature_factor - 50.0f;

	return ret;
}

// See section 7.4 of the datasheet.
bool aht20_init() {
	LOG_DEBUG("initializing AHT20 sensor");

	u8 buf = COMMAND_GET_STATUS;
	TRY(send(&buf, 1))

	u8 status;
	TRY(get_status(&status))
	// Apparently we're supposed to initialize some registers if this isn't true.
	// We haven't implemented that, and it's never been an issue.
	TRY((status & STATUS_INITIAL_EXPECTED_MASK) == STATUS_INITIAL_EXPECTED_MASK)

	return true;
}

// See section 7.4 of the datasheet.
bool aht20_measure(struct aht20_data* const ret) {
	LOG_DEBUG("measuring from sensor");

	u8 buf[7];

	buf[0] = COMMAND_MEASURE;
	buf[1] = 0x33;
	buf[2] = 0x00;
	TRY(send(buf, 3))

	sleep_micros(80'000);
	wait_until_not_busy();

	TRY(recv(buf, 7))

	*ret = decode_sensor_data(buf);
	LOG_DEBUG("sensor data: humidity %f, temperature %f", ret->humidity, ret->temperature);
	return true;
}
