#pragma once

enum {
	i2c_speed_slow = 10'000,
	i2c_speed_standard = 100'000,
	i2c_speed_fast = 400'000,
	i2c_speed_fast_plus = 1'000'000,
};

typedef u8 i2c_address_t;

void i2c_init(u32 clock_speed);
// returns true on success
bool i2c_recv(i2c_address_t address, u8* buf, u32 len);
// returns true on success
bool i2c_send(i2c_address_t address, u8 const* buf, u32 len);
