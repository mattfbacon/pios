#include "base.h"
#include "log.h"
#include "mailbox.h"
#include "try.h"

u32 volatile __attribute__((aligned(16))) mailbox[64];

static struct {
	u32 read;
	u32 _res0[5];
	u32 status;
	u32 config;
	u32 write;
} volatile* const VIDEOCORE_MAILBOX = (void volatile*)(PERIPHERAL_BASE + 0xb880);

enum {
	MAILBOX_RESPONSE = 1u << 31,
	STATUS_WRITE_FULL = 1 << 31,
	STATUS_READ_EMPTY = 1 << 30,
};

bool mailbox_call(mailbox_channel_t const channel) {
	LOG_DEBUG("mailbox call to channel %u", channel);

	u32 const command = (u32)(usize)&mailbox | (u32)(channel & 0b1111);

	while (VIDEOCORE_MAILBOX->status & STATUS_WRITE_FULL) {
		asm volatile("isb");
	}

	VIDEOCORE_MAILBOX->write = command;

	while (true) {
		while (VIDEOCORE_MAILBOX->status & STATUS_READ_EMPTY) {
			asm volatile("isb");
		}

		if (VIDEOCORE_MAILBOX->read == command) {
			return mailbox[1] == MAILBOX_RESPONSE;
		}
	}
}

bool mailbox_get_clock_rate(mailbox_clock_t const clock, u32* const ret) {
	LOG_DEBUG("getting rate of clock %u", clock);

	mailbox[0] = 8 * sizeof(u32);
	mailbox[1] = MAILBOX_REQUEST;
	mailbox[2] = MAILBOX_TAG_GET_CLOCK_RATE;
	mailbox[3] = 2 * sizeof(u32);
	mailbox[4] = 0;
	mailbox[5] = clock;
	// `mailbox[6]` will be set to the rate.
	mailbox[7] = MAILBOX_TAG_LAST;

	TRY(mailbox_call(mailbox_channel_tags))

	u32 const rate = mailbox[6];
	LOG_TRACE("clock %u rate has been set to %u", clock, rate);
	*ret = rate;

	return true;
}

bool mailbox_set_clock_rate(mailbox_clock_t const clock, u32 const rate) {
	LOG_DEBUG("setting clock %u rate to %u", clock, rate);

	mailbox[0] = 9 * sizeof(u32);
	mailbox[1] = MAILBOX_REQUEST;
	mailbox[2] = MAILBOX_TAG_SET_CLOCK_RATE;
	mailbox[3] = 3 * sizeof(u32);
	mailbox[4] = 0;
	mailbox[5] = clock;
	mailbox[6] = rate;
	// "Skip setting turbo" flag. Irrelevant for our use case.
	mailbox[7] = 0;
	mailbox[8] = MAILBOX_TAG_LAST;

	return mailbox_call(mailbox_channel_tags);
}
