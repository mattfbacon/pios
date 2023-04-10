// # References
//
// - <https://en.wikipedia.org/wiki/GUID_Partition_Table>
// - <https://developer.apple.com/library/archive/technotes/tn2166/_index.html#//apple_ref/doc/uid/DTS10003927-CH1-SUBSECTION11>

#include "emmc.h"
#include "gpt.h"
#include "string.h"
#include "try.h"

enum : u64 {
	GPT_SIGNATURE = 0x5452'4150'2049'4645ull,
};

enum : u32 {
	GPT_REVISION = 0x0001'0000u,

	GPT_HEADER_SIZE = 92,
	GPT_HEADER_LBA = 1,
};

static bool guid_equal(guid_t const* const a, guid_t const* const b) {
	return memcmp(a, b, sizeof(guid_t)) == 0;
}

struct __attribute__((packed)) gpt_header {
	u64 signature;
	u32 revision;
	u32 header_size;
	u32 header_crc32;
	u32 _res0;
	lba_t current_lba;
	lba_t backup_lba;
	lba_t first_usable_lba;
	lba_t last_usable_lba;
	guid_t disk_guid;
	lba_t partitions_table_start_lba;
	u32 number_of_partitions;
	u32 size_of_partitions_table_entry;
	u32 partitions_table_crc32;
};

struct __attribute__((packed)) gpt_partition_entry {
	guid_t partition_type;
	guid_t partition_guid;
	lba_t first_lba;
	lba_t last_lba;
	u64 attribute_flags;
	u16 partition_name[36];
};

bool find_partition_by_guid(guid_t const* const expected_id, struct lba_range* const ret) {
	u8 buf[EMMC_BLOCK_SIZE] __attribute__((aligned(alignof(struct gpt_header))));

	TRY_MSG(emmc_read(buf, GPT_HEADER_LBA, 1))

	_Static_assert(sizeof(struct gpt_header) <= sizeof(buf));
	_Static_assert(alignof(struct gpt_header) <= alignof(buf));
	struct gpt_header const* const header = (struct gpt_header const*)buf;

	TRY_MSG(header->signature == GPT_SIGNATURE)
	TRY_MSG(header->revision == GPT_REVISION)
	TRY_MSG(header->header_size >= GPT_HEADER_SIZE)

	// Could possibly overflow on disks larger than 2.2 TB, but is expected to almost always be simply 2.
	if (header->partitions_table_start_lba > U32_MAX) {
		LOG_ERROR("partition table start LBA is way too big");
		return false;
	}
	u32 const table_base = (u32)header->partitions_table_start_lba;
	u32 const num_entries = header->number_of_partitions;
	u32 const entry_size = header->size_of_partitions_table_entry;
	TRY_MSG(entry_size >= sizeof(struct gpt_partition_entry))
	TRY_MSG(EMMC_BLOCK_SIZE % entry_size == 0)
	u32 const entries_per_block = EMMC_BLOCK_SIZE / entry_size;

	{
		u32 block = table_base;
		u32 entry_idx = 0;
		while (entry_idx < num_entries) {
			assert(emmc_read(buf, block, 1), "reading");
			for (u32 entry_in_block = 0; entry_in_block < entries_per_block; ++entry_in_block) {
				struct gpt_partition_entry const* const entry = (struct gpt_partition_entry const*)&buf[entry_in_block * entry_size];

				if (guid_equal(&entry->partition_guid, expected_id)) {
					*ret = (struct lba_range){
						.first = entry->first_lba,
						.last = entry->last_lba,
					};
					return true;
				}

				++entry_idx;
			}
			++block;
		}
	}

	LOG_ERROR("could not find the partition. you may need to update the expected GUID");
	return false;
}
