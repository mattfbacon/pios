#include "base.h"
#include "gpio.h"

static u32 volatile* const GPIO_BASE = (u32 volatile*)(PERIPHERAL_BASE + 0x200000);

enum {
	MODE = 0,
	SET = 7,
	CLEAR = 10,
	READ = 13,
	PULL = 57,
};

void gpio_set_mode(gpio_pin_t const pin, gpio_mode_t const mode) {
	u32 const reg = MODE + (pin / 10);
	u32 const shift = (pin % 10) * 3;

	u32 const mask = 0b111 << shift;

	u32 value = GPIO_BASE[reg];
	value &= ~mask;
	value |= (u32)mode << shift;
	GPIO_BASE[reg] = value;
}

void gpio_write(gpio_pin_t const pin, bool const value) {
	u32 const reg = (value ? SET : CLEAR) + (pin / 32);
	u32 const bit = pin % 32;
	GPIO_BASE[reg] = 1 << bit;
}

bool gpio_read(gpio_pin_t const pin) {
	u32 const reg = READ + (pin / 32);
	u32 const bit = pin % 32;
	return GPIO_BASE[reg] & (1 << bit);
}

void gpio_set_pull(gpio_pin_t const pin, gpio_pull_t const pull) {
	u32 const reg = PULL + (pin / 16);
	u32 const shift = (pin % 16) * 2;

	u32 const mask = 0b11 << shift;

	u32 value = GPIO_BASE[reg];
	value &= ~mask;
	value |= pull << shift;
	GPIO_BASE[reg] = value;
}
