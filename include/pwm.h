#pragma once

typedef enum pwm_controller {
	pwm_controller_0,
	pwm_controller_1,
} pwm_controller_t;

typedef enum pwm_channel {
	pwm_channel_0,
	pwm_channel_1,
} pwm_channel_t;

typedef enum pwm_channel_init_flags {
	pwm_channel_enabled = 1 << 0,
	// unset = PWM mode
	pwm_channel_mode_serializer = 1 << 1,
	pwm_channel_repeat_last = 1 << 2,
	pwm_channel_silence_high = 1 << 3,
	pwm_channel_invert_polarity = 1 << 4,
	pwm_channel_use_fifo = 1 << 5,
	// unset = PWM algorithm
	pwm_channel_use_m_s_algorithm = 1 << 7,
} pwm_channel_init_flags_t;

void pwm_init_clock(u32 rate);

void pwm_init_channel(pwm_controller_t controller, pwm_channel_t channel, pwm_channel_init_flags_t flags);

void pwm_fifo_clear(pwm_controller_t controller);
// will wait until the FIFO is empty
void pwm_fifo_write(pwm_controller_t controller, u32 value);

void pwm_set_range(pwm_controller_t controller, pwm_channel_t channel, u32 range);
void pwm_set_data(pwm_controller_t controller, pwm_channel_t channel, u32 data);
