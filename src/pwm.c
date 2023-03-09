#include "base.h"
#include "clock.h"
#include "log.h"
#include "pwm.h"

static u32 volatile* const PWM0_BASE = (u32 volatile*)(PERIPHERAL_BASE + 0x20c000);
static u32 volatile* const PWM1_BASE = (u32 volatile*)(PERIPHERAL_BASE + 0x20c800);

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
};

static u32 volatile* controller_base(pwm_controller_t const controller) {
	switch (controller) {
		case pwm_controller_0:
			return PWM0_BASE;
		case pwm_controller_1:
			return PWM1_BASE;
	}
}

static u32 channel_shift(pwm_channel_t const channel) {
	switch (channel) {
		case pwm_channel_0:
			return 0;
		case pwm_channel_1:
			return 8;
	}
}

void pwm_init_clock(u32 const divisor) {
	LOG_TRACE("initializing PWM clock to divisor %u", divisor);
	clock_init(clock_id_pwm, divisor);
}

void pwm_init_channel(pwm_controller_t const controller, pwm_channel_t const channel, pwm_channel_init_flags_t const flags) {
	LOG_DEBUG("initializing PWM%u channel %u with flags %x", controller, channel, flags);
	u32 const shift = channel_shift(channel);
	u32 volatile* const reg = &controller_base(controller)[CONTROL];
	u32 value = *reg;
	value &= ~(CONTROL_MASK << shift);
	value |= flags << shift;
	*reg = value;
}

void pwm_fifo_clear(pwm_controller_t const controller) {
	LOG_TRACE("clearing PWM%u FIFO", controller);
	controller_base(controller)[CONTROL] |= CLEAR_FIFO;
}

void pwm_fifo_write(pwm_controller_t const controller, u32 const value) {
	u32 volatile* const reg = controller_base(controller);

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

void pwm_set_range(pwm_controller_t const controller, pwm_channel_t const channel, u32 const range) {
	LOG_DEBUG("setting PWM%u channel %u range to %u", controller, channel, range);
	controller_base(controller)[CHANNEL0_RANGE + (channel * (CHANNEL1_RANGE - CHANNEL0_RANGE))] = range;
}

void pwm_set_data(pwm_controller_t const controller, pwm_channel_t const channel, u32 const data) {
	LOG_TRACE("setting PWM%u channel %u data to %u", controller, channel, data);
	controller_base(controller)[CHANNEL0_DATA + (channel * (CHANNEL1_DATA - CHANNEL0_DATA))] = data;
}
