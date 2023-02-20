#include "devices/aht20.h"
#include "i2c.h"
#include "init.h"
#include "sleep.h"
#include "uart.h"

static void print_sensor_data(aht20_data_t const data) {
	uart_printf("the temperature is about %u °C and the humidity is about %u%%\r\n", (u32)data.temperature, (u32)data.humidity);
}

void main(void) {
	uart_send_str("initializing i2c\r\n");
	i2c_init(i2c_speed_standard);

	// wait for sensor to initialize
	sleep_micros(100'000);

	uart_send_str("initializing aht20\r\n");
	aht20_init();

	sleep_micros(10'000);

	while (true) {
		uart_send_str("starting measuring\r\n");
		aht20_data_t const data = aht20_measure();
		print_sensor_data(data);
		sleep_micros(5'000'000);
	}
}
