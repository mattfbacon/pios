#include "mailbox.h"

uint32_t volatile __attribute__((aligned(16))) mailbox[36];

static uint32_t volatile* const VIDEOCORE_MAILBOX = (uint32_t volatile*)0xfe00b880;
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
	uint32_t const command = (uint32_t)((uintptr_t)&mailbox) | (uint32_t)(channel & 0b1111);

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
