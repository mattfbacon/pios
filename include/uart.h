#pragma once

void uart_init(void);

bool uart_can_recv(void);
u8 uart_recv(void);

bool uart_can_send(void);
void uart_send(char ch);
void uart_send_str(char const* str);
void uart_printf(char const* fmt, ...);
