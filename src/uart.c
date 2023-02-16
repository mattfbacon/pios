#include "base.h"
#include "gpio.h"
#include "mailbox.h"
#include "printf.h"
#include "uart.h"

// for uart0
static u32 volatile* const BASE = (u32 volatile*)(PERIPHERAL_BASE + 0x201000);

enum {
	TX_PIN = 14,
	RX_PIN = 15,

	FIFO = 0,

	STATUS = 6,
	TRANSMIT_FIFO_FULL = 1 << 5,
	RECEIVE_FIFO_EMPTY = 1 << 4,

	BAUD_INTEGER = 9,
	BAUD_FRACTION = 10,

	LINE_CONTROL = 11,
	ENABLE_FIFOS = 1 << 4,
	WORD_LENGTH_8BIT = 1 << 5 | 1 << 6,

	CONTROL = 12,
	ENABLE_DEVICE = 1 << 0,
	ENABLE_TRANSMIT = 1 << 8,
	ENABLE_RECEIVE = 1 << 9,

	INTERRUPT_MASK = 14,
	MASK_ALL = ~0,

	INTERRUPT_CONTROL = 17,
	CLEAR_INTERRUPTS = 0x7ff,
};

u32 div_rounded(u32 const dividend, u32 const divisor) {
	return (dividend + (divisor / 2)) / divisor;
}

// register value = round((system clock freq (500 mhz) / (desired baud * 8)) - 1)
u32 baud_to_reg_value(u32 const baud) {
	return div_rounded(500000000, baud * 8) - 1;
}

void uart_init(void) {
	gpio_set_mode(TX_PIN, gpio_mode_alt0);
	gpio_set_pull(TX_PIN, gpio_pull_floating);
	gpio_set_mode(RX_PIN, gpio_mode_alt0);
	gpio_set_pull(RX_PIN, gpio_pull_floating);

	BASE[CONTROL] = 0;
	BASE[INTERRUPT_CONTROL] = CLEAR_INTERRUPTS;

	// set uart clock to 3 mhz
	mailbox[0] = 9 * 4;
	mailbox[1] = 0;
	mailbox[2] = 0x38002;
	mailbox[3] = 12;
	mailbox[4] = 8;
	mailbox[5] = 2;
	mailbox[6] = 3'000'000;
	mailbox[7] = 0;
	mailbox[8] = 0;
	mailbox_call(8);

	BASE[BAUD_INTEGER] = 1;
	BASE[BAUD_FRACTION] = 40;

	BASE[LINE_CONTROL] = ENABLE_FIFOS | WORD_LENGTH_8BIT;

	BASE[INTERRUPT_MASK] = MASK_ALL;

	BASE[CONTROL] = ENABLE_DEVICE | ENABLE_RECEIVE | ENABLE_TRANSMIT;
}

void uart_send(char const ch) {
	while (BASE[STATUS] & TRANSMIT_FIFO_FULL) {
		asm volatile("isb");
	}

	BASE[FIFO] = ch;
}

u8 uart_recv(void) {
	while (BASE[STATUS] & RECEIVE_FIFO_EMPTY) {
		asm volatile("isb");
	}

	return BASE[FIFO];
}

void uart_send_str(char const* str) {
	for (; *str != '\0'; ++str) {
		uart_send(*str);
	}
}

void uart_printf(char const* fmt, ...) {
	__builtin_va_list args;
	__builtin_va_start(args, fmt);
	vdprintf(uart_send, fmt, args);
}
