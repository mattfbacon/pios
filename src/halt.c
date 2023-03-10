#include "exception.h"
#include "halt.h"

void halt(void) {
	exception_set_mask(exception_mask_all);
	while (true) {
		asm volatile("wfe");
	}
}
