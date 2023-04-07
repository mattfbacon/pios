// # References
//
// - <https://github.com/ultibohub/Core/blob/e9baa63720f09ce03c362aa8b1cb8256278f2aea/source/rtl/ultibo/core/bcm2711.pas#L12448-L12561>

#include "base.h"
#include "random.h"
#include "sleep.h"

enum {
	CONTROL_DIVIDER_SHIFT = 13,
	CONTROL_DIVIDER = 3 << CONTROL_DIVIDER_SHIFT,
	CONTROL_GENERATE_MASK = (1 << 13) - 1,

	CONTROL_ENABLED = CONTROL_DIVIDER | CONTROL_GENERATE_MASK,
	CONTROL_DISABLED = CONTROL_DIVIDER,

	FIFO_THRESHOLD_SHIFT = 8,
	FIFO_THRESHOLD = 2 << FIFO_THRESHOLD_SHIFT,
	FIFO_COUNT_MASK = 0xff,

	WARMUP_COUNT = 0x40'000,
};

static struct {
	u32 control;
	u32 rng_soft_reset;
	u32 rbg_soft_reset;
	u32 total_bit_count;
	u32 total_bit_count_threshold;
	u32 _res0;
	u32 interrupt_status;
	u32 interrupt_enable;
	u32 fifo_data;
	u32 fifo_count;
} volatile* const base = (void volatile*)(PERIPHERAL_BASE + 0x10'4000);

u32 random(void) {
	base->total_bit_count_threshold = WARMUP_COUNT;
	base->fifo_count = FIFO_THRESHOLD;

	base->control = CONTROL_ENABLED;

	// Ensure we have good data. Initial numbers are less random.
	while (base->total_bit_count <= 16) {
		sleep_micros(SLEEP_MIN_MICROS_FOR_INTERRUPTS);
	}

	// Wait for a value. Usually one is already available.
	while ((base->fifo_count & FIFO_COUNT_MASK) < 1) {
		sleep_micros(SLEEP_MIN_MICROS_FOR_INTERRUPTS);
	}

	u32 const ret = base->fifo_data;

	base->control = CONTROL_DISABLED;

	return ret;
}
