#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef uint8_t mailbox_channel_t;

extern uint32_t volatile mailbox[36];

bool mailbox_call(mailbox_channel_t const channel);
