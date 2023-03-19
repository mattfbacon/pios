#pragma once

#include "i2c.h"

typedef u8 mcp23017_pin_t;

typedef struct mcp23017_device {
	u8 modes[2];
	u8 pulls[2];
	u8 outputs[2];
	i2c_address_t address;
} mcp23017_device_t;

typedef enum mcp23017_mode {
	mcp23017_mode_input = 1,
	mcp23017_mode_output = 0,
} mcp23017_mode_t;

// the MCP23017 only supports pull-up.
typedef enum mcp23017_pull {
	mcp23017_pull_floating = 0,
	mcp23017_pull_up = 1,
} mcp23017_pull_t;

enum {
	MCP23017_ADDRESS = 0x20,
};

void mcp23017_init(mcp23017_device_t* this, i2c_address_t address);

// Returns true on success, false on failure.
bool mcp23017_set_mode(mcp23017_device_t* this, mcp23017_pin_t pin, mcp23017_mode_t mode);

// Returns true on success, false on failure.
bool mcp23017_write(mcp23017_device_t* this, mcp23017_pin_t pin, bool value);

bool mcp23017_write_all(mcp23017_device_t* this, u16 values);

// Returns true on success, false on failure.
bool mcp23017_read(mcp23017_device_t* this, mcp23017_pin_t pin, bool* ret);

// Returns true on success, false on failure.
bool mcp23017_read_all(mcp23017_device_t* this, u16* ret);

// Returns true on success, false on failure.
bool mcp23017_set_pull(mcp23017_device_t* this, mcp23017_pin_t pin, mcp23017_pull_t pull);
