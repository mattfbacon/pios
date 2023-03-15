#pragma once

enum {
	EMMC_BLOCK_SIZE = 512,
};

bool emmc_init(void);
// In blocks.
u32 emmc_capacity(void);
// `num_blocks` can be at most 65535.
bool emmc_read(u8* buffer, u32 block_start, u32 num_blocks);
// `num_blocks` can be at most 65535.
// If `buffer` is `NULL`, zeros will be written instead.
bool emmc_write(u8 const* buffer, u32 block_start, u32 num_blocks);
