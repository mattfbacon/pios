// There are two controllers and two channels.
// Controllers and channels are zero-indexed.

#pragma once

typedef enum pwm_channel_init_flags {
	pwm_channel_enabled = 1 << 0,
	// Unset uses the PWM mode.
	pwm_channel_mode_serializer = 1 << 1,
	pwm_channel_repeat_last = 1 << 2,
	pwm_channel_silence_high = 1 << 3,
	pwm_channel_invert_polarity = 1 << 4,
	pwm_channel_use_fifo = 1 << 5,
	// Unset uses the PWM algorithm.
	pwm_channel_use_m_s_algorithm = 1 << 7,
} pwm_channel_init_flags_t;

void pwm_init_clock(u32 rate);

void pwm_init_channel(u8 controller, u8 channel, pwm_channel_init_flags_t flags);

void pwm_fifo_clear(u8 controller);
// Will wait until the FIFO has space.
void pwm_fifo_write(u8 controller, u32 value);

void pwm_set_range(u8 controller, u8 channel, u32 range);
void pwm_set_data(u8 controller, u8 channel, u32 data);
