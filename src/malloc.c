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

void free(void* address) {
	if (address == NULL) {
		return;
	}

	struct block_header* const block = (struct block_header*)((u8*)address - __builtin_offsetof(struct block_header, data));
	block->_size |= FREE;

	// Merge blocks.
	for (struct block_header* next = BLOCK_NEXT(block); next && BLOCK_FREE(next); next = BLOCK_NEXT(next)) {
		block->_size += BLOCK_SIZE(next) + sizeof(struct block_header);
		block->next = next->next;
	}
}
