#include "pwm.h"

static uint32_t volatile* const PWM0_BASE = (uint32_t volatile*)0xfe20c000;
static uint32_t volatile* const PWM1_BASE = (uint32_t volatile*)0xfe20c800;
static uint32_t volatile* const CLOCK_BASE = (uint32_t volatile*)0xfe101000;

enum {
	CONTROL = 0,
	STATUS = 1,
	CHANNEL0_RANGE = 4,
	CHANNEL0_DATA = 5,
	FIFO = 6,
	CHANNEL1_RANGE = 8,
	CHANNEL1_DATA = 9,

	CONTROL_MASK = 0xff,
	CLEAR_FIFO = 1 << 6,

	FIFO_FULL = 1,
	ERROR_MASK = 0b100111100,

	CLOCK_CONTROL = 40,
	CLOCK_DIVISOR = 41,
	CLOCK_PASSWORD = 0x5a000000,
	CLOCK_BUSY = 1 << 7,
	CLOCK_DISABLE = 1 << 5,
	CLOCK_ENABLE = 1 << 4,
	CLOCK_SOURCE_OSCILLATOR = 1,
};

static uint32_t volatile* controller_base(pwm_controller_t const controller) {
	switch (controller) {
		case pwm_controller_0:
			return PWM0_BASE;
		case pwm_controller_1:
			return PWM1_BASE;
	}
}

static uint32_t channel_shift(pwm_channel_t const channel) {
	switch (channel) {
		case pwm_channel_0:
			return 0;
		case pwm_channel_1:
			return 8;
	}
}

void pwm_init_clock(uint32_t const divisor) {
	// clock must be disabled before its divisor can be changed
	CLOCK_BASE[CLOCK_CONTROL] = CLOCK_PASSWORD | CLOCK_DISABLE;
	while (CLOCK_BASE[CLOCK_CONTROL] & CLOCK_BUSY)
		;

	CLOCK_BASE[CLOCK_DIVISOR] = CLOCK_PASSWORD | (divisor << 12);

	// datasheet says not to enable clock at the same time as setting the source
	CLOCK_BASE[CLOCK_CONTROL] = CLOCK_PASSWORD | CLOCK_SOURCE_OSCILLATOR;
	CLOCK_BASE[CLOCK_CONTROL] = CLOCK_PASSWORD | CLOCK_SOURCE_OSCILLATOR | CLOCK_ENABLE;
}

void pwm_init_channel(pwm_controller_t const controller, pwm_channel_t const channel, pwm_channel_init_flags_t const flags) {
	uint32_t const shift = channel_shift(channel);
	uint32_t volatile* const reg = &controller_base(controller)[CONTROL];
	uint32_t value = *reg;
	value &= ~(CONTROL_MASK << shift);
	value |= flags << shift;
	*reg = value;
}

void pwm_fifo_clear(pwm_controller_t const controller) {
	controller_base(controller)[CONTROL] |= CLEAR_FIFO;
}

void pwm_fifo_write(pwm_controller_t const controller, uint32_t const value) {
	uint32_t volatile* const reg = controller_base(controller);

	// wait for room
	while (reg[STATUS] & FIFO_FULL) {
		asm volatile("isb");
	}

	// actually write
	reg[FIFO] = value;

	// acknowledge any errors
	if (reg[STATUS] & ERROR_MASK) {
		reg[STATUS] = ERROR_MASK;
	}
}

void pwm_set_range(pwm_controller_t const controller, pwm_channel_t const channel, uint32_t const range) {
	controller_base(controller)[CHANNEL0_RANGE + (channel * (CHANNEL1_RANGE - CHANNEL0_RANGE))] = range;
}

void pwm_set_data(pwm_controller_t const controller, pwm_channel_t const channel, uint32_t const data) {
	controller_base(controller)[CHANNEL0_DATA + (channel * (CHANNEL1_DATA - CHANNEL0_DATA))] = data;
}
