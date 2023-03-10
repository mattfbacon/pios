#include "base.h"
#include "log.h"
#include "mailbox.h"
#include "pwm.h"

struct pwm_regs {
	u32 control;
	u32 status;
	u32 dma;
	u32 _res0;
	u32 range0;
	u32 data0;
	u32 fifo;
	u32 _res1;
	u32 range1;
	u32 data1;
};

static struct pwm_regs volatile* const PWM0_BASE = (struct pwm_regs volatile*)(PERIPHERAL_BASE + 0x20c000);
static struct pwm_regs volatile* const PWM1_BASE = (struct pwm_regs volatile*)(PERIPHERAL_BASE + 0x20c800);

enum {
	CONTROL_MASK = 0xff,
	CONTROL_CLEAR_FIFO = 1 << 6,

	STATUS_FIFO_FULL = 1,
	STATUS_ERROR_MASK = 0b100111100,
};

static struct pwm_regs volatile* controller_base(pwm_controller_t const controller) {
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

void pwm_init_clock(u32 const rate) {
	LOG_TRACE("initializing PWM clock to rate %u", rate);
	mailbox_set_clock_rate(mailbox_clock_pwm, rate);
}

void pwm_init_channel(pwm_controller_t const controller, pwm_channel_t const channel, pwm_channel_init_flags_t const flags) {
	LOG_DEBUG("initializing PWM%u channel %u with flags %x", controller, channel, flags);
	u32 const shift = channel_shift(channel);
	u32 volatile* const reg = &controller_base(controller)->control;
	u32 value = *reg;
	value &= ~(CONTROL_MASK << shift);
	value |= flags << shift;
	*reg = value;
}

void pwm_fifo_clear(pwm_controller_t const controller) {
	LOG_TRACE("clearing PWM%u FIFO", controller);
	controller_base(controller)->control |= CONTROL_CLEAR_FIFO;
}

void pwm_fifo_write(pwm_controller_t const controller, u32 const value) {
	struct pwm_regs volatile* const reg = controller_base(controller);

	// wait for room
	while (reg->status & STATUS_FIFO_FULL) {
		asm volatile("isb");
	}

	// actually write
	reg->fifo = value;

	// acknowledge any errors
	reg->status = STATUS_ERROR_MASK;
}

void pwm_set_range(pwm_controller_t const controller, pwm_channel_t const channel, u32 const range) {
	LOG_DEBUG("setting PWM%u channel %u range to %u", controller, channel, range);
	struct pwm_regs volatile* const base = controller_base(controller);
	u32 volatile* const reg = channel ? &base->range1 : &base->range0;
	*reg = range;
}

void pwm_set_data(pwm_controller_t const controller, pwm_channel_t const channel, u32 const data) {
	LOG_TRACE("setting PWM%u channel %u data to %u", controller, channel, data);
	struct pwm_regs volatile* const base = controller_base(controller);
	u32 volatile* const reg = channel ? &base->data1 : &base->data0;
	*reg = data;
}
