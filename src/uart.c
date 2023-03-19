// # Implementation Notes
//
// The UART peripheral does provide interrupt-based operation.
// However, in FIFO mode the "can transmit" and "can receive" interrupts can only be triggered when the FIFO is 1/8 full at the lowest.
// For interrupt-based operation to work for us it would need to send an interrupt when any data can be transmitted/received.
// Note that if the FIFO is disabled the interrupts are triggered how we would like. However, the data loss is not acceptable.
// Instead, we use `sleep_micros` and are careful to use interrupt-based, rather than spin-based, sleeping.
//
// There's no logging in this module because logging goes over the UART; that would be quite recursive.

#include "base.h"
#include "gpio.h"
#include "halt.h"
#include "mailbox.h"
#include "printf.h"
#include "sleep.h"
#include "uart.h"

static struct {
	u32 fifo;
	u32 _res0[5];
	u32 status;
	u32 _res1[2];
	// divisor = divisor_integer + divisor_fraction / 64
	// divisor = base clock / (16 * desired baud rate)
	u32 baud_divisor_integer;
	u32 baud_divisor_fraction;
	u32 line_control;
	u32 control;
	u32 interrupt_fifo_level;
	u32 interrupt_enable;
	u32 raw_interrupt_status;
	u32 masked_interrupt_status;
	u32 interrupt_clear;
	// There are more registers but we don't use them.
} volatile* const UART0 = (void volatile*)(PERIPHERAL_BASE + 0x201'000);

static bool initialized = false;

enum {
	// This cannot necessarily just be set to any value.
	// There is a certain minimum for the hardware clock rate somewhere between 460800 and 921600.
	// Below that point, you must use the UART's dividers to further lower the clock rate.
	// For example, to get 9600 baud, you could keep the current clock rate, but set the divisor to 115200 / 9600 = 12.
	BAUD = 115'200,

	TX_PIN = 14,
	RX_PIN = 15,

	STATUS_TRANSMIT_FIFO_FULL = 1 << 5,
	STATUS_RECEIVE_FIFO_EMPTY = 1 << 4,

	LINE_CONTROL_ENABLE_FIFOS = 1 << 4,
	LINE_CONTROL_WORD_LENGTH_8BIT = 1 << 5 | 1 << 6,

	CONTROL_ENABLE_DEVICE = 1 << 0,
	CONTROL_ENABLE_TRANSMIT = 1 << 8,
	CONTROL_ENABLE_RECEIVE = 1 << 9,
};

void uart_init(void) {
	gpio_set_mode(TX_PIN, gpio_mode_alt0);
	gpio_set_pull(TX_PIN, gpio_pull_floating);
	gpio_set_mode(RX_PIN, gpio_mode_alt0);
	gpio_set_pull(RX_PIN, gpio_pull_floating);

	UART0->control = 0;
	UART0->interrupt_clear = 0xffff'ffff;

	mailbox_set_clock_rate(mailbox_clock_uart, BAUD * 16);

	UART0->baud_divisor_integer = 1;
	UART0->baud_divisor_fraction = 0;

	UART0->line_control = LINE_CONTROL_ENABLE_FIFOS | LINE_CONTROL_WORD_LENGTH_8BIT;

	UART0->control = CONTROL_ENABLE_DEVICE | CONTROL_ENABLE_RECEIVE | CONTROL_ENABLE_TRANSMIT;
	initialized = true;
}

bool uart_can_send(void) {
	return !initialized || !(UART0->status & STATUS_TRANSMIT_FIFO_FULL);
}

void uart_send(char const ch) {
	if (!initialized) {
		return;
	}

	while (!uart_can_send()) {
		sleep_micros(SLEEP_MIN_MICROS_FOR_INTERRUPTS);
	}

	UART0->fifo = ch;
}

bool uart_can_recv(void) {
	return initialized && !(UART0->status & STATUS_RECEIVE_FIFO_EMPTY);
}

u8 uart_recv(void) {
	if (!initialized) {
		halt();
	}

	while (!uart_can_recv()) {
		sleep_micros(SLEEP_MIN_MICROS_FOR_INTERRUPTS);
	}

	return UART0->fifo;
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
