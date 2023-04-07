#pragma once

typedef struct __attribute__((packed)) guid {
	u32 a;
	u16 b, c;
	u8 d[2];
	u8 e[6];
} guid_t;
_Static_assert(sizeof(guid_t) == 16);

typedef u64 lba_t;

// Represents the range `start..=end`.
struct lba_range {
	lba_t first;
	lba_t last;
};

bool find_partition_by_guid(guid_t const* expected_id, struct lba_range* ret);
