#pragma once

typedef u8 gpio_pin_t;

typedef enum gpio_mode : u32 {
	gpio_mode_input = 0,
	gpio_mode_output = 1,
	gpio_mode_alt0 = 4,
	gpio_mode_alt1 = 5,
	gpio_mode_alt2 = 6,
	gpio_mode_alt3 = 7,
	gpio_mode_alt4 = 3,
	gpio_mode_alt5 = 2,
} gpio_mode_t;

typedef enum gpio_pull : u32 {
	gpio_pull_floating = 0,
	gpio_pull_up = 1,
	gpio_pull_down = 2,
} gpio_pull_t;

void gpio_set_mode(gpio_pin_t pin, gpio_mode_t mode);
void gpio_write(gpio_pin_t pin, bool value);
bool gpio_read(gpio_pin_t pin);
void gpio_set_pull(gpio_pin_t pin, gpio_pull_t pull);
