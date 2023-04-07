// # IRQ Controller
//
// I could not find any documentation for the interrupt controller's MMIO register layout but LLD contains code for it so I based this implementation on that.
// Reference part 13 for more information.
//
// This is not the GIC, the new interrupt controller available in the RPi 4.
// There is so little information available online about the GIC that I gave up and simply used the legacy controller.
// Configuring the hardware to use the legacy controller is relatively simple: just don't initialize the GIC. I commented out the call to the initialization function in the armstub.
//
// # IRQ Numbers
//
// The IRQ numbers can be found at heading 6.2.4, page 87, of the BCM2711 peripherals datasheet.
// As each register contains 32 bits, numbers 32..=63 are at bit `number - 32` in the second register for the desired function, so for example to enable interrupt number 50 we would do `IRQ_BASE->irq0_enable[1] |= 1 << (50 - 32);`.
//
// # DAIF
//
// This module also contains code to manage the DAIF status register.
// DAIF is a bit confusing because there are two ways to access/modify it: directly as the DAIF MSR and indirectly as DAIFCLR and DAIFSET.
//
// When using the DAIF register directly, bits 6..=9 are used, while when using DAIFCLR and DAIFSET, bits 0..=3 are used.
//
// There are four bits in the mask, D, A, I, and F:
// - (bit 3/9) D = debug exceptions
// - (bit 2/8) A = SError (system error) exceptions
// - (bit 1/7) I = IRQs
// - (bit 0/6) F = FIQs
//
// When a bit is set in the DAIF, it is masked, so that type of exception will not be received.
//
// In our general usage we want SErrors and IRQs, so we clear those bits in `exception_init`.

#include "base.h"
#include "exception.h"
#include "halt.h"
#include "log.h"
#include "timer.h"

// These functions are only exposed to assembly, so we put their prototypes here.
void __attribute__((noreturn)) exception_handle_invalid(u64 index);
void exception_handle_el1_irq(void);

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

void exception_handle_invalid(u64 const index) {
	u64 current_el, syndrome, address, fault_address;
	asm("mrs %0, CurrentEL" : "=r"(current_el));
	asm("mrs %0, esr_el1" : "=r"(syndrome));
	asm("mrs %0, elr_el1" : "=r"(address));
	asm("mrs %0, far_el1" : "=r"(fault_address));
	LOG_FATAL("invalid exception caught, current EL = %u, index = %x, syndrome = 0x%x, address = 0x%x, fault address = 0x%x", current_el >> 2, index, syndrome, address, fault_address);
	halt();
}

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
