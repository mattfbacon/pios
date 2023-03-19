// # References
//
// - <https://github.com/isometimes/rpi4-osdev/tree/master/part5-framebuffer>

#include "framebuffer.h"
#include "log.h"
#include "mailbox.h"

struct framebuffer {
	u32 width;
	u32 height;
	u32 stride;
	framebuffer_color_t volatile* buffer;
};

static struct framebuffer framebuffer = {};

void framebuffer_init(void) {
	LOG_DEBUG("initializing framebuffer");

	u32 const desired_width = 1920;
	u32 const desired_height = 1080;

	mailbox[0] = 35 * 4;
	mailbox[1] = MAILBOX_REQUEST;

	mailbox[2] = MAILBOX_TAG_SET_PHYSICAL_WIDTH_HEIGHT;
	mailbox[3] = 8;
	mailbox[4] = 8;
	mailbox[5] = desired_width;
	mailbox[6] = desired_height;

	mailbox[7] = MAILBOX_TAG_SET_VIRTUAL_WIDTH_HEIGHT;
	mailbox[8] = 8;
	mailbox[9] = 8;
	mailbox[10] = desired_width;
	mailbox[11] = desired_height;

	mailbox[12] = MAILBOX_TAG_SET_VIRTUAL_OFFSET;
	mailbox[13] = 8;
	mailbox[14] = 8;
	mailbox[15] = 0;
	mailbox[16] = 0;

	mailbox[17] = MAILBOX_TAG_SET_DEPTH;
	mailbox[18] = 4;
	mailbox[19] = 4;
	mailbox[20] = 32;

	mailbox[21] = MAILBOX_TAG_SET_PIXEL_ORDER;
	mailbox[22] = 4;
	mailbox[23] = 4;
	mailbox[24] = 1;

	mailbox[25] = MAILBOX_TAG_GET_FRAMEBUFFER;
	mailbox[26] = 8;
	mailbox[27] = 8;
	mailbox[28] = 4096;
	mailbox[29] = 0;

	mailbox[30] = MAILBOX_TAG_GET_STRIDE;
	mailbox[31] = 4;
	mailbox[32] = 4;
	mailbox[33] = 0;

	mailbox[34] = MAILBOX_TAG_LAST;

	// Check call is successful and we have a pointer with depth 32.
	if (mailbox_call(mailbox_channel_tags) && mailbox[20] == 32 && mailbox[28] != 0) {
		framebuffer.width = mailbox[10];
		framebuffer.height = mailbox[11];
		// XXX will stride ever not be a multiple of 4??
		framebuffer.stride = mailbox[33] / sizeof(framebuffer_color_t);
		// Convert GPU address to ARM address.
		framebuffer.buffer = (framebuffer_color_t volatile*)(usize)(mailbox[28] & 0x3fffffff);
	}
}

void framebuffer_draw_pixel(u32 const x, u32 const y, framebuffer_color_t const color) {
	if (y > framebuffer.height || x > framebuffer.width) {
		return;
	}
	framebuffer.buffer[y * framebuffer.stride + x] = color;
}

framebuffer_color_t framebuffer_current(u32 const x, u32 const y) {
	if (y > framebuffer.height || x > framebuffer.width) {
		return 0;
	}
	return framebuffer.buffer[y * framebuffer.stride + x];
}

void framebuffer_fill(framebuffer_color_t const color) {
	for (u32 y = 0; y < framebuffer.height; ++y) {
		for (u32 x = 0; x < framebuffer.width; ++x) {
			framebuffer.buffer[y * framebuffer.stride + x] = color;
		}
	}
}
