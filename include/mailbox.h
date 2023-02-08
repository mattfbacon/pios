#pragma once

typedef u8 mailbox_channel_t;

extern u32 volatile mailbox[36];

bool mailbox_call(mailbox_channel_t const channel);
