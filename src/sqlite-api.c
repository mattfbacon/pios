// # References
//
// (These references are for anything related to embedding SQLite.)
//
// - <https://www.sqlite.org/c3ref/vfs.html>
// - <https://www.sqlite.org/c3ref/initialize.html>
// - <https://www.sqlite.org/malloc.html>
// - <https://www.sqlite.org/c3ref/io_methods.html>
// - <https://www.sqlite.org/lockingv3.html#how_to_corrupt>
// - <https://www.sqlite.org/custombuild.html>
//
// # Notes
//
// Don't forget to execute `pragma journal_mode=memory` before interacting with the database. Otherwise there will be an error.

#include <sqlite3.h>
#include <time.h>

#include "devices/ds3231.h"
#include "emmc.h"
#include "random.h"
#include "sleep.h"
#include "sqlite-api.h"
#include "string.h"
#include "time.h"
#include "try.h"

enum : usize {
	MAX_PATHNAME = 16,
};

enum : u32 {
	// This is totally random.
	FILE_INFO_SIGNATURE = 0xfd51'ba38,
};

struct lba_range sqlite_database_partition;

struct __attribute__((packed)) file_info_on_disk {
	u32 signature;
	u32 version;
	u64 size;
};

struct file_info {
	u64 size;
};

struct file {
	sqlite3_file base;
	struct file_info info;
};

static u8 shared_buffer[EMMC_BLOCK_SIZE] __attribute__((aligned(alignof(struct file_info_on_disk))));

static bool write_file_info(struct file_info const* const info) {
	memset(shared_buffer, 0, sizeof(shared_buffer));
	struct file_info_on_disk* const on_disk = (struct file_info_on_disk*)&shared_buffer;
	on_disk->signature = FILE_INFO_SIGNATURE;
	on_disk->version = 0;
	on_disk->size = info->size;
	TRY_MSG(emmc_write(shared_buffer, (u32)sqlite_database_partition.first, 1))
	return true;
}

static bool read_file_info(struct file_info* const ret) {
	TRY_MSG(emmc_read(shared_buffer, (u32)sqlite_database_partition.first, 1))
	struct file_info_on_disk* const info = (struct file_info_on_disk*)&shared_buffer;

	if (info->signature != FILE_INFO_SIGNATURE) {
		LOG_INFO("invalid file signature, assuming not initialized and overwriting");
		ret->size = 0;
		TRY_MSG(write_file_info(ret))
		return true;
	}

	switch (info->version) {
		case 0:
			break;
		// If more versions are added, the last `case` will break and the previous will perform the necessary migrations sequentially and then write back the updated info.
		default:
			LOG_ERROR("unrecognized info header version %u", info->version);
			return false;
	}

	ret->size = info->size;
	return true;
}

static int main_db_close(sqlite3_file* const file_) {
	struct file* const file = (struct file*)file_;

	bool const success = write_file_info(&file->info);
	return success ? SQLITE_OK : SQLITE_IOERR_CLOSE;
}

struct divided_operation {
	// Simply the length in bytes of the addressed region.
	u64 length;
	// Exclusive. The offset of first byte after the operation that will not be accessed.
	u64 end_byte;
	u32 partial_start_bytes;
	u32 first_full_block;
	u32 num_full_blocks;
	u32 partial_end_bytes;
};

static bool convert_length_and_offset(i64 const offset_, i64 const length_, struct divided_operation* const ret) {
	LOG_DEBUG("convert_length_and_offset(offset=%d, length=%d)", offset_, length_);

	if (length_ < 0 || offset_ < 0) {
		LOG_ERROR("length %d or offset %d negative", length_, offset_);
		return false;
	}

	u64 const length = (u64)length_;
	u64 const offset = (u64)offset_;

	u32 const partition_data_start = (u32)(sqlite_database_partition.first + 1);
	u32 const first_full_block = (u32)((offset + EMMC_BLOCK_SIZE - 1) / EMMC_BLOCK_SIZE) + partition_data_start;
	if (first_full_block < partition_data_start) {
		LOG_ERROR("offset %u overflowed", offset);
		return false;
	}

	u32 partial_start_bytes = EMMC_BLOCK_SIZE - (offset % EMMC_BLOCK_SIZE);
	if (partial_start_bytes == EMMC_BLOCK_SIZE) {
		partial_start_bytes = 0;
	}

	u64 const length_without_start = length - partial_start_bytes;

	u32 const num_full_blocks = (u32)(length_without_start / EMMC_BLOCK_SIZE);
	u32 const partial_end_bytes = length_without_start % EMMC_BLOCK_SIZE;
	u32 const last_block = first_full_block + num_full_blocks + (partial_end_bytes > 0 ? 1 : 0) - 1;

	if (last_block > sqlite_database_partition.last) {
		LOG_ERROR("trying to access beyond end of partition");
		return false;
	}

	ret->length = length;
	ret->end_byte = offset + length;
	ret->partial_start_bytes = partial_start_bytes;
	ret->first_full_block = first_full_block;
	ret->num_full_blocks = num_full_blocks;
	ret->partial_end_bytes = partial_end_bytes;

	LOG_DEBUG(
		"convert_length_and_offset returns length=%u, end_byte=%u, partial_start_bytes=%u, first_full_block=%u, num_full_blocks=%u, partial_end_bytes=%u",
		(u64)ret->length,
		(u64)ret->end_byte,
		(u64)ret->partial_start_bytes,
		(u64)ret->first_full_block,
		(u64)ret->num_full_blocks,
		(u64)ret->partial_end_bytes);

	return true;
}

static int main_db_read(sqlite3_file* const file_, void* const buffer_, int const length_, sqlite3_int64 const offset_) {
	LOG_DEBUG("main_db_read(%d, %d)", length_, offset_);

	struct file* const file = (struct file*)file_;
	u8* const buffer = buffer_;

	struct divided_operation divided;
	if (!convert_length_and_offset(offset_, length_, &divided)) {
		return SQLITE_MISUSE;
	}

	if (divided.end_byte > file->info.size) {
		LOG_WARN("trying to read beyond end of file: file size is %llu but end byte is %llu", file->info.size, divided.end_byte);
		memset(buffer, 0, divided.length);
		return SQLITE_IOERR_SHORT_READ;
	}

	if (divided.num_full_blocks == (u32)-1) {
		if (!emmc_read(shared_buffer, divided.first_full_block - 1, 1)) {
			return SQLITE_IOERR_READ;
		}

		memcpy(buffer, shared_buffer + (EMMC_BLOCK_SIZE - divided.partial_start_bytes), divided.length);

		return SQLITE_OK;
	}

	if (divided.partial_start_bytes != 0) {
		LOG_DEBUG("reading %u partial start bytes", divided.partial_start_bytes);
		if (!emmc_read(shared_buffer, divided.first_full_block - 1, 1)) {
			return SQLITE_IOERR_READ;
		}
		memcpy(buffer, shared_buffer + (EMMC_BLOCK_SIZE - divided.partial_start_bytes), divided.partial_start_bytes);
	}

	if (!emmc_read(buffer + divided.partial_start_bytes, divided.first_full_block, divided.num_full_blocks)) {
		return SQLITE_IOERR_READ;
	}

	if (divided.partial_end_bytes != 0) {
		LOG_DEBUG("reading %u partial end bytes", divided.partial_end_bytes);
		if (!emmc_read(shared_buffer, divided.first_full_block + divided.num_full_blocks, 1)) {
			return SQLITE_IOERR_READ;
		}
		memcpy(buffer + divided.length - divided.partial_end_bytes, shared_buffer, divided.partial_end_bytes);
	}

	return SQLITE_OK;
}

static int main_db_write(sqlite3_file* const file_, void const* const buffer_, int const length_, sqlite3_int64 const offset_) {
	LOG_DEBUG("main_db_write(%u, %u)", length_, offset_);

	struct file* const file = (struct file*)file_;
	u8 const* const buffer = buffer_;

	struct divided_operation divided;
	if (!convert_length_and_offset(offset_, length_, &divided)) {
		return SQLITE_MISUSE;
	}

	if (divided.end_byte > file->info.size) {
		// `convert_length_and_offset` would have failed if this will go beyond the end of the partition.
		LOG_ERROR("write increases size of file to %u", divided.end_byte);
		file->info.size = divided.end_byte;
		if (!write_file_info(&file->info)) {
			return SQLITE_IOERR_WRITE;
		}
	}

	if (divided.num_full_blocks == (u32)-1) {
		memcpy(shared_buffer + (EMMC_BLOCK_SIZE - divided.partial_start_bytes), buffer, divided.length);

		if (!emmc_write(shared_buffer, divided.first_full_block - 1, 1)) {
			return SQLITE_IOERR_WRITE;
		}

		return SQLITE_OK;
	}

	if (divided.partial_start_bytes != 0) {
		LOG_DEBUG("writing %u partial start bytes", divided.partial_start_bytes);
		// Partial writes trash the other data in the block.
		memset(shared_buffer, 0, sizeof(shared_buffer));
		memcpy(shared_buffer + (EMMC_BLOCK_SIZE - divided.partial_start_bytes), buffer, divided.partial_start_bytes);
		if (!emmc_write(shared_buffer, divided.first_full_block - 1, 1)) {
			return SQLITE_IOERR_WRITE;
		}
	}

	if (!emmc_write(buffer + divided.partial_start_bytes, divided.first_full_block, divided.num_full_blocks)) {
		return SQLITE_IOERR_WRITE;
	}

	if (divided.partial_end_bytes != 0) {
		LOG_DEBUG("writing %u partial end bytes", divided.partial_end_bytes);
		memset(shared_buffer, 0, sizeof(shared_buffer));
		memcpy(shared_buffer, buffer + divided.length - divided.partial_end_bytes, divided.partial_end_bytes);
		if (!emmc_write(shared_buffer, divided.first_full_block + divided.num_full_blocks, 1)) {
			return SQLITE_IOERR_WRITE;
		}
	}

	return SQLITE_OK;
}

static int main_db_truncate(sqlite3_file* const file_, sqlite3_int64 const new_size_) {
	LOG_DEBUG("main_db_truncate(%d)", new_size_);

	struct file* const file = (struct file*)file_;

	if (new_size_ < 0) {
		LOG_ERROR("new size %d negative", new_size_);
		return SQLITE_MISUSE;
	}
	u64 const new_size = (u64)new_size_;

	u64 const old_size = file->info.size;
	if (new_size > old_size) {
		// Since we zero out the rest of the contents during partial writes, we only need to worry about full blocks here.
		u32 const old_num_blocks = (u32)((old_size + EMMC_BLOCK_SIZE - 1) / EMMC_BLOCK_SIZE);
		u32 const new_num_blocks = (u32)((new_size + EMMC_BLOCK_SIZE - 1) / EMMC_BLOCK_SIZE);
		if (!emmc_write(NULL, old_num_blocks, new_num_blocks - old_num_blocks)) {
			return SQLITE_IOERR_TRUNCATE;
		}
	}

	file->info.size = new_size;

	if (old_size != new_size) {
		if (!write_file_info(&file->info)) {
			return SQLITE_IOERR_TRUNCATE;
		}
	}

	return SQLITE_OK;
}

static int main_db_sync(sqlite3_file*, int) {
	LOG_DEBUG("main_db_sync");

	// We have nothing to sync.
	return SQLITE_OK;
}

static int main_db_file_size(sqlite3_file* const file_, sqlite3_int64* const size_ret) {
	LOG_DEBUG("main_db_file_size");

	struct file* const file = (struct file*)file_;

	*size_ret = (i64)file->info.size;
	LOG_DEBUG("main_db_file_size returns %u", file->info.size);

	return SQLITE_OK;
}

static int main_db_lock(sqlite3_file*, int) {
	LOG_DEBUG("main_db_lock");

	return SQLITE_OK;
}

static int main_db_unlock(sqlite3_file*, int) {
	LOG_DEBUG("main_db_unlock");

	return SQLITE_OK;
}

static int main_db_check_reserved_lock(sqlite3_file*, int* const ret) {
	LOG_DEBUG("main_db_check_reserved_lock");

	*ret = SQLITE_LOCK_NONE;
	return SQLITE_OK;
}

static int main_db_file_control(sqlite3_file*, int const op, void*) {
	LOG_DEBUG("main_db_file_control(%d)", op);

	return SQLITE_NOTFOUND;
}

static int main_db_sector_size(sqlite3_file*) {
	LOG_DEBUG("main_db_sector_size");

	return EMMC_BLOCK_SIZE;
}

static int main_db_device_characteristics(sqlite3_file*) {
	LOG_DEBUG("main_db_device_characteristics");

	return SQLITE_IOCAP_ATOMIC | SQLITE_IOCAP_SAFE_APPEND | SQLITE_IOCAP_SEQUENTIAL | SQLITE_IOCAP_UNDELETABLE_WHEN_OPEN;
}

static sqlite3_io_methods const main_db_file = {
	.iVersion = 1,
	.xClose = main_db_close,
	.xRead = main_db_read,
	.xWrite = main_db_write,
	.xTruncate = main_db_truncate,
	.xSync = main_db_sync,
	.xFileSize = main_db_file_size,
	.xLock = main_db_lock,
	.xUnlock = main_db_unlock,
	.xCheckReservedLock = main_db_check_reserved_lock,
	.xFileControl = main_db_file_control,
	.xSectorSize = main_db_sector_size,
	.xDeviceCharacteristics = main_db_device_characteristics,
	// Below methods are not used for version 1.
	.xShmMap = NULL,
	.xShmLock = NULL,
	.xShmBarrier = NULL,
	.xShmUnmap = NULL,
	.xFetch = NULL,
	.xUnfetch = NULL,
};

static int vfs_open(sqlite3_vfs*, sqlite3_filename filename, sqlite3_file* const file_, int const flags, int* const) {
	LOG_DEBUG("vfs_open(%s, %x)", filename, (u64)flags);
	assert(sqlite_database_partition.first > 0 && sqlite_database_partition.last > 0, "sqlite_database_partition must be initialized");
	assert(sqlite_database_partition.first <= sqlite_database_partition.last, "sqlite_database_partition last must be after first");
	assert(sqlite_database_partition.first <= U32_MAX, "sqlite_database_partition first must fit in a u32");
	assert(sqlite_database_partition.last <= U32_MAX, "sqlite_database_partition last must fit in a u32");

	struct file* const file = (struct file*)file_;

	if (flags & SQLITE_OPEN_MAIN_DB) {
		LOG_DEBUG("vfs_open returning main DB");
		file->base.pMethods = &main_db_file;
		if (!read_file_info(&file->info)) {
			return SQLITE_IOERR;
		}
		return SQLITE_OK;
	} else {
		LOG_DEBUG("vfs_open returning a dummy");
		file->base.pMethods = NULL;
		return SQLITE_CANTOPEN;
	}
}

static int vfs_delete(sqlite3_vfs*, char const* filename, int) {
	LOG_WARN("vfs_delete(%s) ignored", filename);
	return SQLITE_OK;
}

static int vfs_access(sqlite3_vfs*, char const* name, int const flags, int* const ret) {
	LOG_DEBUG("vfs_access(%s, %d)", name, flags);

	*ret = strcmp(name, "maindb") == 0;

	return SQLITE_OK;
}

static int vfs_full_pathname(sqlite3_vfs*, char const* name, int, char* const ret) {
	LOG_DEBUG("vfs_full_pathname(%s)", name);
	{
		usize i = 0;
		for (; i < MAX_PATHNAME - 1 && name[i] != '\0'; ++i) {
			ret[i] = name[i];
		}
		ret[i] = '\0';
	}
	return SQLITE_OK;
}

static void* vfs_dlopen(sqlite3_vfs*, char const*) {
	LOG_ERROR("vfs_dlopen not implemented");
	return NULL;
}

static int vfs_randomness(sqlite3_vfs*, int const length, char* const buffer) {
	LOG_DEBUG("vfs_randomness");
	u32 rand_val = 0, rand_bytes_left = 0;
	for (int i = 0; i < length; ++i) {
		if (rand_bytes_left == 0) {
			rand_val = random();
			rand_bytes_left = 4;
		}
		buffer[i] = (u8)rand_val;
		rand_val >>= 8;
		--rand_bytes_left;
	}
	return length;
}

static int vfs_sleep(sqlite3_vfs*, int const micros) {
	LOG_DEBUG("vfs_sleep(%d)", (i64)micros);
	sleep_micros((u64)micros);
	return SQLITE_OK;
}

static int vfs_current_time(sqlite3_vfs*, double*) {
	LOG_ERROR("vfs_current_time not implemented");
	return SQLITE_ERROR;
}

static int vfs_current_time_int64(sqlite3_vfs*, sqlite3_int64*) {
	LOG_ERROR("vfs_current_time_int64 not implemented");
	return SQLITE_ERROR;
}

static sqlite3_vfs vfs = {
	.iVersion = 2,
	.szOsFile = sizeof(struct file),
	.mxPathname = MAX_PATHNAME,
	.pNext = NULL,
	.zName = "mine",
	.pAppData = NULL,
	.xOpen = vfs_open,
	.xDelete = vfs_delete,
	.xAccess = vfs_access,
	.xFullPathname = vfs_full_pathname,
	.xDlOpen = vfs_dlopen,
	// These `dl*` functions will never be called because `dlopen` always fails.
	.xDlError = NULL,
	.xDlSym = NULL,
	.xDlClose = NULL,
	.xRandomness = vfs_randomness,
	.xSleep = vfs_sleep,
	.xCurrentTime = vfs_current_time,
	.xGetLastError = NULL,
	.xCurrentTimeInt64 = vfs_current_time_int64,
	// Below methods are not used for version 2.
	.xSetSystemCall = NULL,
	.xGetSystemCall = NULL,
	.xNextSystemCall = NULL,
};

int sqlite3_os_init(void) {
	LOG_DEBUG("sqlite3_os_init");
	return sqlite3_vfs_register(&vfs, true);
}

int sqlite3_os_end(void) {
	return SQLITE_OK;
}
