#pragma once

// If a given bit is set, that exception is masked, meaning it will not be delivered.
typedef enum exception_mask {
	exception_mask_fiq = 1 << 6,
	exception_mask_irq = 1 << 7,
	exception_mask_error = 1 << 8,
	exception_mask_debug = 1 << 9,
	exception_mask_all = 0b1111 << 6,
} exception_mask_t;

void exception_init(void);
exception_mask_t exception_get_mask(void);
void exception_set_mask(exception_mask_t mask);

inline void _without_interrupts_impl(exception_mask_t const* mask) {
	if (*mask & exception_mask_irq) {
		asm volatile("msr daifclr, #0b0010");
	}
}

#define WITHOUT_INTERRUPTS(_block) \
	do { \
		exception_mask_t const _mask __attribute__((cleanup(_without_interrupts_impl))) = exception_get_mask(); \
		asm volatile("msr daifset, #0b0010"); \
		(void)_mask; \
		{ _block }; \
	} while (false);
