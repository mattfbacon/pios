#include "base.h"
#include "exception.h"
#include "halt.h"
#include "log.h"
#include "timer.h"

enum {
	IRQ0_TIMER0 = 1 << 0,
	IRQ0_TIMER1 = 1 << 1,
	IRQ0_TIMER2 = 1 << 2,
	IRQ0_TIMER3 = 1 << 3,
	IRQ0_AUX = 1 << 29,

	IRQ1_I2C = 1 << (53 - 32),
	IRQ1_UARTS = 1 << (57 - 32),
	IRQ1_EMMC = 1 << (62 - 32),
};

static struct {
	u32 irq0_pending[3];
	u32 res0;
	u32 irq0_enable[3];
	u32 res1;
	u32 irq0_disable[3];
} volatile* const IRQ_BASE = (void volatile*)(PERIPHERAL_BASE + 0xb200);

static void init_controller(void) {
	IRQ_BASE->irq0_enable[0] = IRQ0_TIMER1;
}

void exception_init(void) {
	asm volatile("adr x0, exception_vector_table\nmsr vbar_el1, x0" ::: "x0");
	init_controller();
	asm volatile("msr daifclr, #0b0110");
}

// only exposed to assembly
void exception_handle_invalid(u64 const index, u64 const syndrome, u64 const address) {
	LOG_FATAL("invalid exception caught, index = %x, syndrome = 0x%x, address = 0x%x", index, syndrome, address);
	halt();
}

// only exposed to assembly
void exception_handle_el1_irq(void) {
	u32 const pending0 = IRQ_BASE->irq0_pending[0];

	if (pending0 & IRQ0_TIMER1) {
		timer_acknowledge(1);
	}
}

exception_mask_t exception_get_mask(void) {
	u64 ret;
	asm volatile("mrs %0, daif" : "=r"(ret));
	return (exception_mask_t)ret;
}

void exception_set_mask(exception_mask_t const mask) {
	asm volatile("msr daif, %0" ::"r"((u64)mask));
}
