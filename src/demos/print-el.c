#include "uart.h"

static u8 get_el(void) {
	u64 ret;
	asm("mrs %0, CurrentEL" : "=r"(ret));
	return (u8)((ret >> 2) & 0x3);
}

void main(void) {
	uart_printf("current EL: %u\r\n", get_el());
}
