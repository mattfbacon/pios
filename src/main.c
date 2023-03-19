#include "malloc.h"
#include "uart.h"

void main(void) {
	char* str1 = malloc(16);
	uart_printf("str1=0x%x\r\n", str1);

	free(str1);
	uart_send_str("freed\r\n");

	str1 = malloc(16);
	uart_printf("re1, str1=0x%x\r\n", str1);

	char* str2 = malloc(32);
	uart_printf("str2=0x%x\r\n", str2);

	free(str1);
	free(str2);
	uart_send_str("freed\r\n");

	str1 = malloc(48);
	uart_printf("re2 str1=0x%x\r\n", str1);
}
