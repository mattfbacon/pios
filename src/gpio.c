#include <stddef.h>

#include "gpio.h"

static uint32_t volatile* const GPIO_BASE = (uint32_t volatile*)0xfe200000;

void gpio_set_mode(gpio_pin_t const pin, gpio_mode_t const mode) {
	size_t const reg = pin / 10;
	size_t const shift = (pin % 10) * 3;

	uint32_t const mask = 0b111 << shift;

	uint32_t value = GPIO_BASE[reg];
	value &= ~mask;
	value |= mode << shift;
	GPIO_BASE[reg] = value;
}

void gpio_write(gpio_pin_t const pin, bool const value) {
	size_t const reg = (pin / 32) + (value ? 7 : 10);
	size_t const bit = pin % 32;
	GPIO_BASE[reg] = 1 << bit;
}

bool gpio_read(gpio_pin_t const pin) {
	size_t const reg = (pin / 32) + 13;
	size_t const bit = pin % 32;
	return GPIO_BASE[reg] & (1 << bit);
}

void gpio_set_pull(gpio_pin_t const pin, gpio_pull_t const pull) {
	size_t const reg = 57 + (pin / 16);
	size_t const shift = (pin % 16) * 2;

	uint32_t const mask = 0b11 << shift;

	uint32_t value = GPIO_BASE[reg];
	value &= ~mask;
	value |= pull << shift;
	GPIO_BASE[reg] = value;
}
