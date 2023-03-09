#include "emmc.h"
#include "try.h"
#include "uart.h"

static u8 buf[EMMC_BLOCK_SIZE * 2];
static char const msg[] = "another test";

void main(void) {
	LOG_INFO("starting initialization");
	assert(emmc_init(), "initializing EMMC");
	LOG_INFO("EMMC disk initialized");

	LOG_INFO("reading from card");
	assert(emmc_read(buf, sizeof(buf) / EMMC_BLOCK_SIZE, 2'000'000), "reading from card");

	LOG_INFO("read done, bytes are as follows: %D", buf, sizeof(buf));

	LOG_INFO("modifying data");
	for (usize i = 0; i < sizeof(buf); ++i) {
		buf[i] = msg[i % sizeof(msg)];
	}

	LOG_INFO("writing to card");
	assert(emmc_write(buf, sizeof(buf) / EMMC_BLOCK_SIZE, 2'000'000), "writing to card");

	LOG_INFO("reading back from card");
	assert(emmc_read(buf, sizeof(buf) / EMMC_BLOCK_SIZE, 2'000'000), "reading back from card");

	LOG_INFO("readback done, bytes are as follows: %D", buf, sizeof(buf));
}
