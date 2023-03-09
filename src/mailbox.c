#include "base.h"
#include "log.h"
#include "mailbox.h"
#include "try.h"

u32 volatile __attribute__((aligned(16))) mailbox[64];

static u32 volatile* const VIDEOCORE_MAILBOX = (u32 volatile*)(PERIPHERAL_BASE + 0xb880);
enum {
	MAILBOX_READ = 0,
	MAILBOX_STATUS = 6,
	MAILBOX_CONFIG = 7,
	MAILBOX_WRITE = 8,

	MAILBOX_RESPONSE = 0x8000'0000,
	MAILBOX_FULL = 0x8000'0000,
	MAILBOX_EMPTY = 0x4000'0000,
};

bool mailbox_call(mailbox_channel_t const channel) {
	LOG_DEBUG("mailbox call to channel %u", channel);

	u32 const command = (u32)(usize)&mailbox | (u32)(channel & 0b1111);

	while (VIDEOCORE_MAILBOX[MAILBOX_STATUS] & MAILBOX_FULL) {
		asm volatile("isb");
	}

	VIDEOCORE_MAILBOX[MAILBOX_WRITE] = command;

	while (true) {
		while (VIDEOCORE_MAILBOX[MAILBOX_STATUS] & MAILBOX_EMPTY) {
			asm volatile("isb");
		}

		if (VIDEOCORE_MAILBOX[MAILBOX_READ] == command) {
			return mailbox[1] == MAILBOX_RESPONSE;
		}
	}
}

struct mailbox_command_header {
	u32 id;
	u32 buffer_length;
	u32 value_length;
};

struct mailbox_command_clock {
	struct mailbox_command_header header;
	u32 clock_id;
	u32 out_rate;
};

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
	LOG_TRACE("clock %u rate is %u", clock, rate);
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
