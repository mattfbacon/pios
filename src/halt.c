#include "exception.h"
#include "halt.h"

void halt(void) {
	// Avoid unnecessary wakeups.
	exception_set_mask(exception_mask_all);
	while (true) {
		asm volatile("wfe");
	}
}
