// TODO explore using a doubly linked list for the block list so we can merge blocks backward as well.

#include "base.h"
#include "mailbox.h"
#include "malloc.h"
#include "try.h"

extern char _end[];

enum {
	BLOCK_ALIGNMENT = 8u,
	FREE = 1 << 0,
	ADDRESS_MASK = ~(BLOCK_ALIGNMENT - 1),

	SPLIT_MARGIN = 16,
};

struct block_header {
	// Block sizes are multiples of 8 bytes so the bottom 3 bits are used for flags.
	u32 _size;
	// Our largest pointer is `DEVICE_BASE` which fits in a `u32`.
	u32 next;
	u8 data[];
};

#define BLOCK_SIZE(_block) ((_block)->_size & ADDRESS_MASK)
#define BLOCK_FREE(_block) ((bool)((_block)->_size & FREE))
#define BLOCK_NEXT(_block) ((struct block_header*)(usize)(_block)->next)
#define BLOCK_FROM_DATA(_data) ((struct block_header*)((u8*)(_data) - __builtin_offsetof(struct block_header, data)))

static struct block_header* head_block = NULL;

void malloc_init(void) {
	u32 base, size;
	assert(mailbox_get_arm_memory(&base, &size), "getting ARM memory region");
	u32 const heap_start = (u32)(usize)_end;
	u32 const arm_memory_end = base + size;
	u32 const heap_size = arm_memory_end - heap_start;

	head_block = (struct block_header*)(usize)heap_start;
	head_block->next = (u32)(usize)NULL;
	head_block->_size = ((heap_size - sizeof(struct block_header)) & ADDRESS_MASK) | FREE;
}

static usize align_to(usize value, usize const alignment) {
	value += alignment - 1;
	return value - (value % alignment);
}

void* malloc(usize size) {
	if (size == 0) {
		return NULL;
	}

	size = align_to(size, BLOCK_ALIGNMENT);

	// Try to find an existing block that we can reuse.
	for (struct block_header* block = head_block; block != NULL; block = BLOCK_NEXT(block)) {
		if (!BLOCK_FREE(block) || BLOCK_SIZE(block) < size) {
			continue;
		}

		// Split the block if necessary.
		if (BLOCK_SIZE(block) > size + sizeof(struct block_header) + SPLIT_MARGIN) {
			struct block_header* const new_next = (struct block_header*)(block->data + size);
			new_next->next = block->next;
			new_next->_size = (BLOCK_SIZE(block) - size - sizeof(struct block_header)) | FREE;
			block->next = (u32)(usize)new_next;
			// Also clears the free bit.
			block->_size = size;
		}

		block->_size &= ~FREE;
		return block->data;
	}

	return NULL;
}

static void try_merge(struct block_header* const block) {
	for (struct block_header* next = BLOCK_NEXT(block); next && BLOCK_FREE(next); next = BLOCK_NEXT(next)) {
		block->_size += BLOCK_SIZE(next) + sizeof(struct block_header);
		block->next = next->next;
	}
}

void free(void* address) {
	if (address == NULL) {
		return;
	}

	struct block_header* const block = BLOCK_FROM_DATA(address);
	block->_size |= FREE;

	try_merge(block);
}

void* calloc(usize const num_members, usize const member_size) {
	if (num_members == 0 || member_size == 0) {
		return NULL;
	}

	usize const size = num_members * member_size;

	// A standard way to check for overflow. Optimizes well.
	if (size / num_members != member_size) {
		return NULL;
	}

	u8* const ret = malloc(size);
	if (ret != NULL) {
		for (usize i = 0; i < size; ++i) {
			ret[i] = 0;
		}
	}
	return ret;
}

void* realloc(void* const old, usize new_size) {
	if (old == NULL) {
		return malloc(new_size);
	} else if (new_size == 0) {
		if (old != NULL) {
			free(old);
		}
		return NULL;
	}
	// old != NULL, new_size != 0

	new_size = align_to(new_size, BLOCK_ALIGNMENT);

	struct block_header* const block = BLOCK_FROM_DATA(old);
	// We can merge free blocks ahead of this used block into this block, to possibly give us more capacity. This may merge more than it has to, but then we can just split again.
	try_merge(block);
	if (BLOCK_SIZE(block) >= new_size + sizeof(struct block_header) + SPLIT_MARGIN) {
		// Split.
		struct block_header* const new_next = (struct block_header*)(block->data + new_size);
		new_next->next = block->next;
		new_next->_size = (BLOCK_SIZE(block) - new_size - sizeof(struct block_header)) | FREE;
		try_merge(new_next);
		block->next = (u32)(usize)new_next;
		// Clears the free flag.
		block->_size = new_size;
		return old;
	} else if (BLOCK_SIZE(block) >= new_size) {
		// Keep the old block without splitting.
		return old;
	} else {
		// Make a new allocation and copy.
		u8* const new_raw = malloc(new_size);

		if (new_raw != NULL) {
			usize const old_size = BLOCK_SIZE(block);
			for (usize i = 0; i < old_size; ++i) {
				new_raw[i] = ((u8*)old)[i];
			}
		}

		free(old);

		return new_raw;
	}
}
