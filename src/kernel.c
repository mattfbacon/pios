#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "framebuffer.h"
#include "gpio.h"

/*
static void sleep_cycles(unsigned long cycles) {
  for (; cycles > 0; --cycles) {
    asm volatile("isb");
  }
}
*/

void kernel_main() {
	framebuffer_init();

	framebuffer_fill(0xff0000);
}
