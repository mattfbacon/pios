#include "base.h"
#include "gpio.h"
#include "uart.h"

static u32 volatile* const AUX_BASE = (u32 volatile*)(PERIPHERAL_BASE + 0x215000);

enum {
	TX_PIN = 14,
	RX_PIN = 15,

	ENABLES = 1,
	ENABLE_UART = 1 << 0,

	CONTROL = 24,
	RECEIVER_ENABLE = 1 << 0,
	TRANSMITTER_ENABLE = 1 << 1,

	BAUD = 26,

	LINE_CONTROL = 19,
	MODE_8BIT = 1 << 0,

	LINE_STATUS = 21,
	TRANSMIT_FIFO_NOT_FULL = 1 << 5,
	RECEIVE_FIFO_NOT_EMPTY = 1 << 0,

	FIFO = 16,
};

u32 div_rounded(u32 const dividend, u32 const divisor) {
	return (dividend + (divisor / 2)) / divisor;
}

// register value = round((system clock freq (500 mhz) / (desired baud * 8)) - 1)
u32 baud_to_reg_value(u32 const baud) {
	return div_rounded(500000000, baud * 8) - 1;
}

void uart_init(void) {
	gpio_set_mode(TX_PIN, gpio_mode_alt5);
	gpio_set_pull(TX_PIN, gpio_pull_down);

	gpio_set_mode(RX_PIN, gpio_mode_alt5);
	gpio_set_pull(RX_PIN, gpio_pull_down);

	AUX_BASE[ENABLES] |= ENABLE_UART;

	// temporarily disable for setup
	AUX_BASE[CONTROL] = 0;

	AUX_BASE[BAUD] = baud_to_reg_value(115200);

	AUX_BASE[LINE_CONTROL] = MODE_8BIT;

	AUX_BASE[CONTROL] = RECEIVER_ENABLE | TRANSMITTER_ENABLE;
}

void uart_send(u8 const ch) {
	while (!(AUX_BASE[LINE_STATUS] & TRANSMIT_FIFO_NOT_FULL)) {
		asm volatile("isb");
	}

	AUX_BASE[FIFO] = ch;
}

u8 uart_recv(void) {
	while (!(AUX_BASE[LINE_STATUS] & RECEIVE_FIFO_NOT_EMPTY)) {
		asm volatile("isb");
	}

	return AUX_BASE[FIFO];
}

void uart_send_str(char const* str) {
	for (; *str != '\0'; ++str) {
		uart_send(*str);
	}
}
