#pragma once

typedef enum mailbox_channel {
	mailbox_channel_power = 0,
	mailbox_channel_framebuffer = 1,
	mailbox_channel_virtual_uart = 2,
	mailbox_channel_vchiq = 3,
	mailbox_channel_leds = 4,
	mailbox_channel_buttons = 5,
	mailbox_channel_touchscreen = 6,
	mailbox_channel_counter = 7,
	mailbox_channel_tags = 8,
} mailbox_channel_t;

typedef enum mailbox_clock {
	mailbox_clock_emmc = 1,
	mailbox_clock_uart = 2,
} mailbox_clock_t;

extern u32 volatile mailbox[64];

enum {
	MAILBOX_REQUEST = 0,

	MAILBOX_TAG_GET_CLOCK_RATE = 0x3'0002,
	MAILBOX_TAG_SET_CLOCK_RATE = 0x3'8002,

	MAILBOX_TAG_SET_PHYSICAL_WIDTH_HEIGHT = 0x4'8003,
	MAILBOX_TAG_SET_VIRTUAL_WIDTH_HEIGHT = 0x4'8004,
	MAILBOX_TAG_SET_VIRTUAL_OFFSET = 0x4'8009,
	MAILBOX_TAG_SET_DEPTH = 0x4'8005,
	MAILBOX_TAG_SET_PIXEL_ORDER = 0x4'8006,
	MAILBOX_TAG_GET_FRAMEBUFFER = 0x4'0001,
	MAILBOX_TAG_GET_STRIDE = 0x4'0008,

	MAILBOX_TAG_LAST = 0,
};

bool mailbox_call(mailbox_channel_t const channel);

bool mailbox_get_clock_rate(mailbox_clock_t clock, u32* ret);
bool mailbox_set_clock_rate(mailbox_clock_t clock, u32 rate);
