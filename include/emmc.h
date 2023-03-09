#pragma once

enum {
	EMMC_BLOCK_SIZE = 512,
};

bool emmc_init(void);
// `num_blocks` can be at most 65535.
bool emmc_read(u8* buffer, u32 num_blocks, u32 block_start);
// `num_blocks` can be at most 65535.
bool emmc_write(u8 const* buffer, u32 num_blocks, u32 block_start);
