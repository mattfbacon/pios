// # Approach
//
// We use a traditional "free list" approach for this allocator.
// The name is a bit misleading because the block list contains both free and used blocks.
//
// The block list is a linked list where the items are block headers.
// These headers store links to the previous and next blocks as well as flags for whether the block is free and whether the block is the last block.
// The data region that a given block describes/manages is simply `block->next - block->data` (excluding casts).
// The first block's previous pointer is NULL.
// The last block's next pointer points to the end of the heap, and the block has the last flag set.
//
// In the initial state, the block list contains a single free block spanning the entire region of heap memory.
// When `malloc` is called, it tries to find a suitable block to return, splitting a large block into smaller blocks as necessary.
// This allows the giant initial block to be split into reasonably-sized blocks for allocations as necessary.
//
// Conversely, when `free` is called, beyond simply marking the block as free, we try to merge it with free blocks on either side.
// This allows undoing the splits done by `malloc`, and indeed the heap will return to the initial state of a single giant free block once all allocations have been freed.
// The merging algorithm isn't entirely optimal and unmerged blocks can remain, but it helps a lot compared to not merging at all.
// Besides, these unmerged blocks can still be reused by new properly-sized allocations.
//
// # "First Free Block" Optimization
//
// This allocator also uses a "first free block" optimization.
// When we allocate a block, we know that it won't be free, so there's no reason to make subsequent malloc calls traverse it.
// Thus, if the block we allocated was the current head block, we can set the head block to the block after this one.
//
// Conversely, when freeing a block, if it is before the current head, we can update the head.

#include "base.h"
#include "mailbox.h"
#include "malloc.h"
#include "string.h"
#include "try.h"

extern char _end[];

enum {
	BLOCK_ALIGNMENT = 8u,
	ADDRESS_MASK = ~(BLOCK_ALIGNMENT - 1),

	FLAGS_FREE = 1 << 0,
	FLAGS_LAST = 1 << 1,

	// The minimum block size (as returned by `block_size`) of a split-off block.
	// This is a bit of a heuristic as it's meant to balance block header overhead with the advantage of splitting.
	SPLIT_MARGIN = 16,
};

struct block_header {
	// Our pointers are aligned to 8 bytes so the bottom 3 bits are used for flags.
	u32 prev_and_flags;
	// Our largest possible pointer is `DEVICE_BASE` which fits in a `u32`.
	u32 next;
	u8 data[];
};

static bool block_is_free(struct block_header const* const block) {
	return block->prev_and_flags & FLAGS_FREE;
}

static bool block_is_last(struct block_header const* const block) {
	return block->prev_and_flags & FLAGS_LAST;
}

static struct block_header* block_prev(struct block_header const* const block) {
	return (struct block_header*)(usize)(block->prev_and_flags & ADDRESS_MASK);
}

static struct block_header* block_next(struct block_header const* const block) {
	return block_is_last(block) ? NULL : (struct block_header*)(usize)block->next;
}

static u32 block_size(struct block_header const* const block) {
	return block->next - (u32)(usize)block->data;
}

static struct block_header* block_from_data(void const* const data) {
	return (struct block_header*)((u8*)data - offsetof(struct block_header, data));
}

static void block_set_prev(struct block_header* const restrict block, struct block_header* const restrict new_prev) {
	block->prev_and_flags &= ~ADDRESS_MASK;
	block->prev_and_flags |= (u32)(usize)new_prev;
}

// Inserts `new_block` between `before` and `block_next(before)`.
static void block_insert_after(struct block_header* const restrict before, struct block_header* const restrict new_block) {
	// Link `new_block` and `block_next(before)`.
	new_block->next = before->next;
	struct block_header* const restrict after = block_next(before);
	if (after != NULL) {
		block_set_prev(after, new_block);
	}

	// Link `before` and `new_block`.
	before->next = (u32)(usize)new_block;
	block_set_prev(new_block, before);

	// Update the last flag, if necessary.
	if (after == NULL) {
		before->prev_and_flags &= ~FLAGS_LAST;
		new_block->prev_and_flags |= FLAGS_LAST;
	}
}

static void block_remove(struct block_header* const restrict to_remove) {
	struct block_header* const restrict after = block_next(to_remove);
	struct block_header* const restrict before = block_prev(to_remove);

	// Link `after` to `before`.
	if (after != NULL) {
		block_set_prev(after, before);
	}

	// Link `before` to `after`.
	if (before != NULL) {
		before->next = to_remove->next;
		// Update the last flag, if necessary.
		if (after == NULL) {
			before->prev_and_flags |= FLAGS_LAST;
		}
	}
}

// Returns whether the block was actually split.
static bool try_split(struct block_header* const block, usize const new_size) {
	if (block_size(block) >= new_size + sizeof(struct block_header) + SPLIT_MARGIN) {
		struct block_header* const new_next = (struct block_header*)(block->data + new_size);
		new_next->prev_and_flags = FLAGS_FREE;
		block_insert_after(block, new_next);
		return true;
	} else {
		return false;
	}
}

// Precondition: `block_is_free(block)`.
static struct block_header* try_merge(struct block_header* block, bool const merge_prev) {
	if (merge_prev) {
		while (true) {
			// from: prev -> block -> next
			//   to: prev ----------> next
			struct block_header* const prev = block_prev(block);

			if (prev == NULL || !block_is_free(prev)) {
				break;
			}

			block_remove(block);
			block = prev;
		}
	}

	while (true) {
		// from: block -> next -> next2
		//   to: block ---------> next2
		struct block_header* const next = block_next(block);

		if (next == NULL || !block_is_free(next)) {
			break;
		}

		block_remove(next);
	}

	return block;
}

static struct block_header* head_block = NULL;

static usize align_to(usize value, usize const alignment) {
	value += alignment - 1;
	return value - (value % alignment);
}

void malloc_init(void) {
	u32 base, size;
	assert(mailbox_get_arm_memory(&base, &size), "getting ARM memory region");
	u32 const heap_start = (u32)align_to((usize)_end, BLOCK_ALIGNMENT);
	u32 const arm_memory_end = base + size;

	head_block = (struct block_header*)(usize)heap_start;
	head_block->next = arm_memory_end & ADDRESS_MASK;
	head_block->prev_and_flags = (u32)(usize)NULL | FLAGS_FREE | FLAGS_LAST;
}

void* malloc(usize size) {
	if (size == 0) {
		return NULL;
	}

	size = align_to(size, BLOCK_ALIGNMENT);

	// Try to find an existing block that we can use.
	for (struct block_header* block = head_block; block != NULL; block = block_next(block)) {
		if (!block_is_free(block) || block_size(block) < size) {
			continue;
		}

		try_split(block, size);

		block->prev_and_flags &= ~FLAGS_FREE;
		if (head_block == block) {
			head_block = block_next(block);
		}
		return block->data;
	}

	return NULL;
}

void free(void* const address) {
	if (address == NULL) {
		return;
	}

	struct block_header* const block = block_from_data(address);
	block->prev_and_flags |= FLAGS_FREE;

	if (block < head_block) {
		head_block = block;
	}

	try_merge(block, true);
}

void* calloc(usize const num_members, usize const member_size) {
	if (num_members == 0 || member_size == 0) {
		return NULL;
	}

	usize const size = num_members * member_size;

	// A standard way to check for overflow.
	// Optimizes well, i.e., does not actually perform a division.
	if (size / num_members != member_size) {
		return NULL;
	}

	u8* const ret = malloc(size);
	if (ret != NULL) {
		memset(ret, 0, size);
	}
	return ret;
}

void* realloc(void* const old, usize new_size) {
	// Handle edge/corner cases first.
	if (old == NULL) {
		return malloc(new_size);
	} else if (new_size == 0) {
		if (old != NULL) {
			free(old);
		}
		return NULL;
	}
	// Now in the center case: `old != NULL && new_size != 0`.

	new_size = align_to(new_size, BLOCK_ALIGNMENT);

	struct block_header* const old_block = block_from_data(old);
	usize const old_size = block_size(old_block);

	// Allow `try_merge` to include this block in the merge process.
	old_block->prev_and_flags |= FLAGS_FREE;
	try_merge(old_block, false);

	if (old_size >= new_size) {
		// Keep the old block, possibly splitting if the new size is smaller enough.
		try_split(old_block, new_size);
		old_block->prev_and_flags &= ~FLAGS_FREE;
		return old;
	}

	// Now that we've determined that the current block is not big enough, we can try merging.
	struct block_header* const merged_block = try_merge(old_block, true);
	merged_block->prev_and_flags &= ~FLAGS_FREE;

	if (block_size(merged_block) >= new_size) {
		// We may need to move the data.
		if (merged_block != old_block) {
			memmove(merged_block->data, old_block->data, old_size);
		}

		try_split(merged_block, new_size);

		return merged_block->data;
	}

	// The block was not big enough even after merging with previous blocks.
	// Make a new allocation and copy.
	u8* const new_raw = malloc(new_size);

	// Regardless of whether we merged with previous blocks earlier, the old data will be untouched at its old location.
	// This is because the merged block, which contains the old data, was not marked as free.
	if (new_raw != NULL) {
		memcpy(new_raw, old, old_size);
	}

	// Free the old block, but don't do any merging because this is as merged as it can get.
	merged_block->prev_and_flags |= FLAGS_FREE;
	if (merged_block < head_block) {
		head_block = merged_block;
	}

	return new_raw;
}

usize malloc_usable_size(void* const address) {
	return block_size(block_from_data(address));
}
