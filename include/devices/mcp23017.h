#pragma once

#include "i2c.h"

typedef u8 mcp23017_pin_t;

// returns true on success, false on failure.
bool mcp23017_init(i2c_address_t address);

typedef enum mcp23017_mode {
	mcp23017_mode_input = 1,
	mcp23017_mode_output = 0,
} mcp23017_mode_t;

// returns true on success, false on failure.
bool mcp23017_set_mode(i2c_address_t address, mcp23017_pin_t pin, mcp23017_mode_t mode);

// returns true on success, false on failure.
bool mcp23017_write(i2c_address_t address, mcp23017_pin_t pin, bool value);

// returns true on success, false on failure.
bool mcp23017_read(i2c_address_t address, mcp23017_pin_t pin, bool* ret);

// the MCP23017 only supports pull-up.
typedef enum mcp23017_pull {
	mcp23017_pull_floating = 0,
	mcp23017_pull_up = 1,
} mcp23017_pull_t;

// returns true on success, false on failure.
bool mcp23017_set_pull(i2c_address_t address, mcp23017_pin_t pin, mcp23017_pull_t pull);
