#pragma once

// for a list of the offset of all clocks, see https://github.com/PeterLemon/RaspberryPi/blob/master/Sound/PWM/8BIT/44100Hz/Mono/CPU/LIB/R_PI2.INC

typedef enum clock_id {
	clock_id_pwm = 0x0a0,
} clock_id_t;

void clock_init(clock_id_t clock, u32 divisor);
