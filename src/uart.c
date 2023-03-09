// # Implementation Notes
//
// The UART peripheral does provide interrupt-based operation.
// However, in FIFO mode the "can transmit" and "can receive" interrupts can only be triggered when the FIFO is 1/8 full at the lowest.
// For interrupt-based operation to work for us it would need to send an interrupt when any data can be transmitted/received.
// Note that if the FIFO is disabled the interrupts are triggered how we would like. However, the data loss is not acceptable.
// Instead, we use `sleep_micros` and are careful to use interrupt-based, rather than spin-based, sleeping.
//
// There's no logging in this module because logging goes over the UART; that would be quite recursive.
//
// TODO explore disabling the hardware FIFO and using an interrupt handler + software FIFO to provide this behavior.

#include "base.h"
#include "gpio.h"
#include "mailbox.h"
#include "printf.h"
#include "sleep.h"
#include "uart.h"

// for uart0
static u32 volatile* const BASE = (u32 volatile*)(PERIPHERAL_BASE + 0x201'000);

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

	INTERRUPT_FIFO_LEVEL = 13,
	LEVEL_RECEIVE_1_8 = 0,
	LEVEL_TRANSMIT_1_8 = 0,

	INTERRUPT_ENABLE = 14,
	INTERRUPT_RECEIVE = 1 << 4,
	INTERRUPT_TRANSMIT = 1 << 5,

	INTERRUPT_CONTROL = 17,
	CLEAR_INTERRUPTS = 0x7ff,
};

u32 div_rounded(u32 const dividend, u32 const divisor) {
	return (dividend + (divisor / 2)) / divisor;
}

u32 baud_to_reg_value(u32 const baud) {
	return div_rounded(500'000'000, baud * 8) - 1;
}

void uart_init(void) {
	gpio_set_mode(TX_PIN, gpio_mode_alt0);
	gpio_set_pull(TX_PIN, gpio_pull_floating);
	gpio_set_mode(RX_PIN, gpio_mode_alt0);
	gpio_set_pull(RX_PIN, gpio_pull_floating);

	BASE[CONTROL] = 0;
	BASE[INTERRUPT_CONTROL] = CLEAR_INTERRUPTS;

	mailbox_set_clock_rate(mailbox_clock_uart, 3'000'000);

	BASE[BAUD_INTEGER] = 1;
	BASE[BAUD_FRACTION] = 40;

	BASE[LINE_CONTROL] = ENABLE_FIFOS | WORD_LENGTH_8BIT;

	BASE[INTERRUPT_ENABLE] = 0;

	BASE[CONTROL] = ENABLE_DEVICE | ENABLE_RECEIVE | ENABLE_TRANSMIT;
}

bool uart_can_send(void) {
	return !(BASE[STATUS] & TRANSMIT_FIFO_FULL);
}

void uart_send(char const ch) {
	while (!uart_can_send()) {
		sleep_micros(SLEEP_MIN_MICROS_FOR_INTERRUPTS);
	}

	BASE[FIFO] = ch;
}

bool uart_can_recv(void) {
	return !(BASE[STATUS] & RECEIVE_FIFO_EMPTY);
}

u8 uart_recv(void) {
	while (!uart_can_recv()) {
		sleep_micros(SLEEP_MIN_MICROS_FOR_INTERRUPTS);
	}

	return BASE[FIFO];
}

void uart_send_str(char const* str) {
	for (; *str != '\0'; ++str) {
		uart_send(*str);
	}
}

static void printf_callback(void*, char const ch) {
	uart_send(ch);
}

void uart_vprintf(char const* const fmt, __builtin_va_list const args) {
	vdprintf(printf_callback, NULL, fmt, args);
}

void uart_printf(char const* const fmt, ...) {
	__builtin_va_list args;
	__builtin_va_start(args, fmt);
	uart_vprintf(fmt, args);
	__builtin_va_end(args);
}
