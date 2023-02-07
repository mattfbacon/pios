#include "framebuffer.h"
#include "mailbox.h"

enum {
	MAILBOX_REQUEST = 0,

	MAILBOX_CH_POWER = 0,
	MAILBOX_CH_FB = 1,
	MAILBOX_CH_VUART = 2,
	MAILBOX_CH_VCHIQ = 3,
	MAILBOX_CH_LEDS = 4,
	MAILBOX_CH_BTNS = 5,
	MAILBOX_CH_TOUCH = 6,
	MAILBOX_CH_COUNT = 7,
	MAILBOX_CH_PROP = 8,

	MAILBOX_TAG_SETPOWER = 0x28001,
	MAILBOX_TAG_SETCLKRATE = 0x38002,

	MAILBOX_TAG_SETPHYWH = 0x48003,
	MAILBOX_TAG_SETVIRTWH = 0x48004,
	MAILBOX_TAG_SETVIRTOFF = 0x48009,
	MAILBOX_TAG_SETDEPTH = 0x48005,
	MAILBOX_TAG_SETPXLORDR = 0x48006,
	MAILBOX_TAG_GETFB = 0x40001,
	MAILBOX_TAG_GETSTRIDE = 0x40008,

	MAILBOX_TAG_LAST = 0,
};

struct framebuffer {
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	framebuffer_color_t volatile* buffer;
};

static struct framebuffer framebuffer = {};

void framebuffer_init(void) {
	uint32_t const desired_width = 1920;
	uint32_t const desired_height = 1080;

	mailbox[0] = 35 * 4;
	mailbox[1] = MAILBOX_REQUEST;

	mailbox[2] = MAILBOX_TAG_SETPHYWH;
	mailbox[3] = 8;
	mailbox[4] = 8;
	mailbox[5] = desired_width;
	mailbox[6] = desired_height;

	mailbox[7] = MAILBOX_TAG_SETVIRTWH;
	mailbox[8] = 8;
	mailbox[9] = 8;
	mailbox[10] = desired_width;
	mailbox[11] = desired_height;

	mailbox[12] = MAILBOX_TAG_SETVIRTOFF;
	mailbox[13] = 8;
	mailbox[14] = 8;
	mailbox[15] = 0;
	mailbox[16] = 0;

	mailbox[17] = MAILBOX_TAG_SETDEPTH;
	mailbox[18] = 4;
	mailbox[19] = 4;
	mailbox[20] = 32;

	mailbox[21] = MAILBOX_TAG_SETPXLORDR;
	mailbox[22] = 4;
	mailbox[23] = 4;
	mailbox[24] = 1;

	mailbox[25] = MAILBOX_TAG_GETFB;
	mailbox[26] = 8;
	mailbox[27] = 8;
	mailbox[28] = 4096;
	mailbox[29] = 0;

	mailbox[30] = MAILBOX_TAG_GETSTRIDE;
	mailbox[31] = 4;
	mailbox[32] = 4;
	mailbox[33] = 0;

	mailbox[34] = MAILBOX_TAG_LAST;

	// Check call is successful and we have a pointer with depth 32
	if (mailbox_call(MAILBOX_CH_PROP) && mailbox[20] == 32 && mailbox[28] != 0) {
		framebuffer.width = mailbox[10];
		framebuffer.height = mailbox[11];
		framebuffer.stride = mailbox[33] / sizeof(framebuffer_color_t);  // XXX will stride ever not be a multiple of 4??
		// convert GPU address to ARM address
		framebuffer.buffer = (framebuffer_color_t volatile*)(uintptr_t)(mailbox[28] & 0x3fffffff);
	}
}

void framebuffer_draw_pixel(unsigned int const x, unsigned int const y, framebuffer_color_t const color) {
	if (y > framebuffer.height || x > framebuffer.width) {
		return;
	}
	framebuffer.buffer[y * framebuffer.stride + x] = color;
}

framebuffer_color_t framebuffer_current(unsigned int const x, unsigned int const y) {
	if (y > framebuffer.height || x > framebuffer.width) {
		return 0;
	}
	return framebuffer.buffer[y * framebuffer.stride + x];
}

void framebuffer_fill(framebuffer_color_t const color) {
	for (uint32_t y = 0; y < framebuffer.height; ++y) {
		for (uint32_t x = 0; x < framebuffer.width; ++x) {
			framebuffer.buffer[y * framebuffer.stride + x] = color;
		}
	}
}
