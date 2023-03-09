#include "gpio.h"
#include "pwm.h"
#include "sleep.h"

enum {
	PIN_AUDIO1 = 40,
	PIN_AUDIO2 = 41,
};

static u32 g(u32 const i, u32 const x, u32 const t, u32 const o) {
	return ((3 & x & (i * ((3 & i >> 16 ? "BY}6YB6$" : "Qj}6jQ6%")[t % 8] + 51) >> o)) << 4);
}

void main(void) {
	gpio_set_pull(PIN_AUDIO1, gpio_pull_floating);
	gpio_set_mode(PIN_AUDIO1, gpio_mode_alt0);
	gpio_set_pull(PIN_AUDIO2, gpio_pull_floating);
	gpio_set_mode(PIN_AUDIO2, gpio_mode_alt0);

	pwm_init_clock(2);
	pwm_init_channel(pwm_controller_1, pwm_channel_0, pwm_channel_enabled | pwm_channel_use_fifo);
	pwm_init_channel(pwm_controller_1, pwm_channel_1, pwm_channel_enabled | pwm_channel_use_fifo);
	pwm_fifo_clear(pwm_controller_1);

	// gets us to 8000 hz
	u32 const range = 3375;
	pwm_set_range(pwm_controller_1, pwm_channel_0, range);
	pwm_set_range(pwm_controller_1, pwm_channel_1, range);

	// This song is called "Bitshift Variations in C Minor" if you're curious. It's a really cool way of generating a fun song in a very short amount of code.
	for (u32 i = 0;; ++i) {
		u32 const n = i >> 14;
		u32 const s = i >> 14;
		u8 const this_data = g(i, 1, n, 12) + g(i, s, n ^ i >> 13, 10) + g(i, s / 3, n + ((i >> 11) % 3), 10) + g(i, s / 5, 8 + n - ((i >> 10) % 3), 9);

		// the audio jack has two channels so we write the data twice.
		pwm_fifo_write(pwm_controller_1, this_data);
		pwm_fifo_write(pwm_controller_1, this_data);
	}
}
