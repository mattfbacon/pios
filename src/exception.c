#include "base.h"
#include "halt.h"
#include "timer.h"
#include "uart.h"

enum {
	IRQ0_TIMER0 = 1 << 0,
	IRQ0_TIMER1 = 1 << 1,
	IRQ0_TIMER2 = 1 << 2,
	IRQ0_TIMER3 = 1 << 3,
	IRQ0_AUX = 1 << 29,

	IRQ1_I2C = 1 << (53 - 32),
	IRQ1_UARTS = 1 << (57 - 32),
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
	uart_printf("invalid exception caught\r\nindex = %u\r\nsyndrome = 0x%x\r\naddress = 0x%x\r\n", index, syndrome, address);
	halt();
}

// only exposed to assembly
void exception_handle_el1_irq(void) {
	u32 const pending0 = IRQ_BASE->irq0_pending[0];

	if (pending0 & IRQ0_TIMER1) {
		timer_acknowledge(1);
	}
}
