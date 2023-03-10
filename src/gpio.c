#include "base.h"
#include "gpio.h"
#include "log.h"

static struct {
	u32 mode[3];
	u32 _res0[4];
	u32 set[3];
	u32 clear[3];
	u32 read[3];
	u32 _res1[41];
	u32 pull[3];
} volatile* const GPIO_BASE = (void volatile*)(PERIPHERAL_BASE + 0x20'0000);

void gpio_set_mode(gpio_pin_t const pin, gpio_mode_t const mode) {
	LOG_DEBUG("setting pin %u to mode %u", pin, mode);

	u32 volatile* const reg = &GPIO_BASE->mode[pin / 10];
	u32 const shift = (pin % 10) * 3;
	u32 const mask = 0b111 << shift;

	u32 value = *reg;
	value &= ~mask;
	value |= (u32)mode << shift;
	*reg = value;
}

void gpio_write(gpio_pin_t const pin, bool const value) {
	LOG_DEBUG("writing %B to pin %u", value, pin);

	u32 volatile* const reg = &(value ? GPIO_BASE->set : GPIO_BASE->clear)[pin / 32];
	u32 const bit = pin % 32;
	*reg = 1 << bit;
}

bool gpio_read(gpio_pin_t const pin) {
	LOG_TRACE("reading from pin %u", pin);

	u32 const reg = GPIO_BASE->read[pin / 32];
	u32 const bit = pin % 32;
	bool const set = reg & (1 << bit);
	LOG_DEBUG("read value %B from pin %u", set, pin);
	return set;
}

void gpio_set_pull(gpio_pin_t const pin, gpio_pull_t const pull) {
	LOG_DEBUG("setting pull of pin %u to %u", pin, pull);

	u32 volatile* const reg = &GPIO_BASE->pull[pin / 16];
	u32 const shift = (pin % 16) * 2;

	u32 const mask = 0b11 << shift;

	u32 value = *reg;
	value &= ~mask;
	value |= pull << shift;
	*reg = value;
}
