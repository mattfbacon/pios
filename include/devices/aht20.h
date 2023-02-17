#pragma once

typedef struct aht20_data {
	f32 humidity;
	f32 temperature;
} aht20_data_t;

// don't forget to initialize I2C first.
// wait until 100 ms after system startup before calling this.
void aht20_init(void);

// wait until 10 ms after calling `aht20_init` before calling this.
aht20_data_t aht20_measure(void);
