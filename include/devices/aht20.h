#pragma once

struct aht20_data {
	// Relative humidity, percent.
	f32 humidity;
	// Celsius.
	f32 temperature;
};

// Don't forget to initialize I2C first.
// Wait until 100 ms after system startup before calling this.
bool aht20_init(void);

// Wait until 10 ms after calling `aht20_init` before calling this.
bool aht20_measure(struct aht20_data* ret);
