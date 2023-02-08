#include "base.h"
#include "mailbox.h"

u32 volatile __attribute__((aligned(16))) mailbox[36];

static u32 volatile* const VIDEOCORE_MAILBOX = (u32 volatile*)(PERIPHERAL_BASE + 0xb880);
enum {
	MAILBOX_READ = 0,
	MAILBOX_POLL = 4,
	MAILBOX_SENDER = 5,
	MAILBOX_STATUS = 6,
	MAILBOX_CONFIG = 7,
	MAILBOX_WRITE = 8,

	MAILBOX_RESPONSE = 0x80000000,
	MAILBOX_FULL = 0x80000000,
	MAILBOX_EMPTY = 0x40000000,
};

bool mailbox_call(mailbox_channel_t const channel) {
	u32 const command = (u32)(usize)&mailbox | (u32)(channel & 0b1111);

	while (VIDEOCORE_MAILBOX[MAILBOX_STATUS] & MAILBOX_FULL)
		;

	VIDEOCORE_MAILBOX[MAILBOX_WRITE] = command;

	while (true) {
		while (VIDEOCORE_MAILBOX[MAILBOX_STATUS] & MAILBOX_EMPTY)
			;

		if (VIDEOCORE_MAILBOX[MAILBOX_READ] == command) {
			return mailbox[1] == MAILBOX_RESPONSE;
		}
	}
}
