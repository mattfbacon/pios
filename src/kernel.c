#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "framebuffer.h"
#include "gpio.h"
#include "sleep.h"

static uint32_t volatile* const PWM0_BASE = (uint32_t volatile*)0xfe20c000;
static uint32_t volatile* const PWM1_BASE = (uint32_t volatile*)0xfe20c800;
static uint32_t volatile* const CLOCK_BASE = (uint32_t volatile*)0xfe101000;

enum {
	PWM_CONTROL = 0,
	PWM_STATUS = 1,
	PWM_CHANNEL1_RANGE = 4,
	PWM_CHANNEL1_DATA = 5,
	PWM_FIFO = 6,
	PWM_CHANNEL2_RANGE = 8,
	PWM_CHANNEL2_DATA = 9,

	PWM_CHANNEL1_ENABLE = 1,
	PWM_CHANNEL1_USEFIFO = 1 << 5,
	PWM_CHANNEL2_ENABLE = 1 << 8,
	PWM_CHANNEL2_USEFIFO = 1 << 13,
	PWM_CLEARFIFO = 1 << 6,

	PWM_FIFOFULL = 1,
	PWM_ERRORMASK = 0b100111100,

	CLOCK_CONTROL = 40,
	CLOCK_DIVISOR = 41,
	CLOCK_PASSWORD = 0x5a000000,
	CLOCK_BUSY = 1 << 7,
	CLOCK_DISABLE = 1 << 5,
	CLOCK_ENABLE = 1 << 4,
	CLOCK_SOURCE_OSCILLATOR = 1,
};

static void io_wait(void) {
	sleep_micros(2);
}

static int g(int i, int x, int t, int o) {
	return ((3 & x & (i * ((3 & i >> 16 ? "BY}6YB6$" : "Qj}6jQ6%")[t % 8] + 51) >> o)) << 4);
}

void kernel_main(void) {
	gpio_set_mode(42, gpio_mode_output);

	// set up GPIO

	gpio_set_pull(40, gpio_pull_floating);
	gpio_set_mode(40, gpio_mode_alt0);
	gpio_set_pull(41, gpio_pull_floating);
	gpio_set_mode(41, gpio_mode_alt0);
	io_wait();

	// set up clock

	// stop clock
	CLOCK_BASE[CLOCK_CONTROL] = CLOCK_PASSWORD | (1 << 5);
	while (CLOCK_BASE[CLOCK_CONTROL] & CLOCK_BUSY)
		;

	uint32_t const divisor = 2;
	CLOCK_BASE[CLOCK_DIVISOR] = CLOCK_PASSWORD | (divisor << 12);
	// datasheet says not to enable clock at the same time as setting the source..
	CLOCK_BASE[CLOCK_CONTROL] = CLOCK_PASSWORD | CLOCK_SOURCE_OSCILLATOR;
	CLOCK_BASE[CLOCK_CONTROL] = CLOCK_PASSWORD | CLOCK_SOURCE_OSCILLATOR | CLOCK_ENABLE;

	// set up PWM

	PWM1_BASE[PWM_CONTROL] = 0;
	io_wait();

	PWM1_BASE[PWM_CHANNEL1_RANGE] = PWM1_BASE[PWM_CHANNEL2_RANGE] = 3375;
	PWM1_BASE[PWM_CONTROL] = PWM_CHANNEL1_ENABLE | PWM_CHANNEL1_USEFIFO | PWM_CHANNEL2_ENABLE | PWM_CHANNEL2_USEFIFO | PWM_CLEARFIFO;

	gpio_write(42, true);

	for (int i = 0;; ++i) {
		int const n = i >> 14;
		int const s = i >> 14;
		uint8_t const this_data = g(i, 1, n, 12) + g(i, s, n ^ i >> 13, 10) + g(i, s / 3, n + ((i >> 11) % 3), 10) + g(i, s / 5, 8 + n - ((i >> 10) % 3), 9);

		// wait for room
		while (PWM1_BASE[PWM_STATUS] & PWM_FIFOFULL) {
			asm volatile("isb");
		}

		// write once for each channel; mono
		PWM1_BASE[PWM_FIFO] = this_data;
		PWM1_BASE[PWM_FIFO] = this_data;

		// acknowledge any errors
		if (PWM1_BASE[PWM_STATUS] & PWM_ERRORMASK) {
			PWM1_BASE[PWM_STATUS] = PWM_ERRORMASK;
		}
	}

	/*
	gpio_pin_t const act_led_pin = 42;
	gpio_set_mode(act_led_pin, gpio_mode_output);
	while (true) {
	  gpio_write(act_led_pin, true);
	  sleep_micros(500000);
	  gpio_write(act_led_pin, false);
	  sleep_micros(500000);
	}
	*/

	/*
	gpio_pin_t const pwm_pin = 13;
	gpio_set_pull(pwm_pin, gpio_pull_floating);
	gpio_set_mode(pwm_pin, gpio_mode_alt0);

	CLOCK_BASE[CLOCK_CONTROL] = CLOCK_PASSWORD | CLOCK_DISABLE;
	while (CLOCK_BASE[CLOCK_CONTROL] & CLOCK_BUSY)
	  ;

	uint32_t const divisor = 2;
	CLOCK_BASE[CLOCK_DIVISOR] = CLOCK_PASSWORD | (divisor << 12);
	// datasheet says not to enable clock at the same time as setting the source..
	CLOCK_BASE[CLOCK_CONTROL] = CLOCK_PASSWORD | CLOCK_SOURCE_OSCILLATOR;
	CLOCK_BASE[CLOCK_CONTROL] = CLOCK_PASSWORD | CLOCK_SOURCE_OSCILLATOR | CLOCK_ENABLE;

	PWM0_BASE[PWM_CONTROL] = 0;
	io_wait();

	PWM0_BASE[PWM_CHANNEL1_RANGE] = PWM0_BASE[PWM_CHANNEL2_RANGE] = 1000;
	PWM0_BASE[PWM_CHANNEL1_DATA] = PWM0_BASE[PWM_CHANNEL2_DATA] = 1000;
	PWM0_BASE[PWM_CONTROL] = PWM_CHANNEL1_ENABLE | PWM_CHANNEL2_ENABLE;
	*/

	/*
	*(uint32_t*)(0xe0) = (uintptr_t)&core_main;
	asm volatile("sev");
	while (true) {
	  asm volatile("isb");
	}
	*/

	/*
	framebuffer_init();
	framebuffer_fill(0xff0000);

	gpio_pin_t const act_led_pin = 42;
	gpio_set_mode(act_led_pin, gpio_mode_output);
	while (true) {
	  gpio_write(act_led_pin, true);
	  sleep_micros(500000);
	  gpio_write(act_led_pin, false);
	  sleep_micros(500000);
	}
	*/
}
