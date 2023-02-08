#pragma once

void uart_init(void);
u8 uart_recv(void);
void uart_send(u8 ch);
void uart_send_str(char const* str);
