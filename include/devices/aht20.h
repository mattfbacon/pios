#pragma once

typedef struct aht20_data {
	// Relative humidity, percent.
	f32 humidity;
	// Celsius.
	f32 temperature;
} aht20_data_t;

// Don't forget to initialize I2C first.
// Wait until 100 ms after system startup before calling this.
void aht20_init(void);

// Wait until 10 ms after calling `aht20_init` before calling this.
aht20_data_t aht20_measure(void);
