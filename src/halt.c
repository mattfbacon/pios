#include "halt.h"

void halt(void) {
	// mask IRQs so `wfe` doesn't finish as often
	asm volatile("msr daifset, #0b0010");
	while (true) {
		asm volatile("wfe");
	}
}
