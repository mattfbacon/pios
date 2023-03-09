#include "log.h"

static u8 get_el(void) {
	u64 ret;
	asm("mrs %0, CurrentEL" : "=r"(ret));
	return (u8)((ret >> 2) & 0x3);
}

void main(void) {
	LOG_INFO("current EL: %u", get_el());
}
