#include "base.h"

enum {
	PAGE_SHIFT = 12,
	TABLE_SHIFT = 9,
	SECTION_SHIFT = PAGE_SHIFT + TABLE_SHIFT,
	PAGE_SIZE = 1 << PAGE_SHIFT,
	SECTION_SIZE = 1 << SECTION_SHIFT,
	ENTRIES_PER_TABLE = 512,
	NUM_PMDS = 4,

	// Granule size = 4k
	TCR_TG0_4K = 0 << 14,
	// The lower VA range (starting at 0) extends for 2^48 bytes, or 0xffff'ffff'ffff.
	TCR_T0SZ = 64 - 48,
	TCR_EL1_VALUE = TCR_TG0_4K | TCR_T0SZ,

	MATTR_DEVICE_nGnRnE = 0x0,
	MATTR_NORMAL_NC = 0x44,
	MATTR_DEVICE_nGnRnE_INDEX = 0,
	MATTR_NORMAL_NC_INDEX = 1,
	MAIR_EL1_VALUE = (MATTR_NORMAL_NC << (8 * MATTR_NORMAL_NC_INDEX)) | (MATTR_DEVICE_nGnRnE << (8 * MATTR_DEVICE_nGnRnE_INDEX)),

	TD_VALID = 1 << 0,
	TD_BLOCK = 0 << 1,
	TD_TABLE = 1 << 1,
	TD_ACCESS = 1 << 10,
	TD_KERNEL_PERMS = 1l << 54,
	TD_INNER_SHAREABLE = 3 << 8,

	TD_KERNEL_TABLE_FLAGS = TD_TABLE | TD_VALID,
	TD_KERNEL_BLOCK_FLAGS = TD_ACCESS | TD_INNER_SHAREABLE | TD_KERNEL_PERMS | (MATTR_NORMAL_NC_INDEX << 2) | TD_BLOCK | TD_VALID,
	TD_DEVICE_BLOCK_FLAGS = TD_ACCESS | TD_INNER_SHAREABLE | TD_KERNEL_PERMS | (MATTR_DEVICE_nGnRnE_INDEX << 2) | TD_BLOCK | TD_VALID,

	PGD_SHIFT = PAGE_SHIFT + 3 * TABLE_SHIFT,
	PUD_SHIFT = PAGE_SHIFT + 2 * TABLE_SHIFT,
	PMD_SHIFT = PAGE_SHIFT + TABLE_SHIFT,
	PUD_ENTRY_MAP_SIZE = 1 << PUD_SHIFT,

	BLOCK_SIZE = 0x4000'0000,

	SCTLR_MMU_ENABLED = 1 << 0,
	SCTLR_DATA_CACHEABLE = 1 << 2,
	SCTLR_INSTRUCTIONS_CACHEABLE = 1 << 12,
};

typedef u64 entry_t;
typedef entry_t table_t[ENTRIES_PER_TABLE];
_Static_assert(sizeof(table_t) == PAGE_SIZE);
typedef u64 virtual_addr_t;
typedef u64 physical_addr_t;

static void create_table_entry(table_t table, void const* const next_table, virtual_addr_t const virtual_address, u64 const shift, u64 const flags) {
	u64 const table_index = (virtual_address >> shift) & (ENTRIES_PER_TABLE - 1);
	u64 const descriptor = (u64)next_table | flags;
	table[table_index] = descriptor;
}

static void create_block_map(table_t pmd, virtual_addr_t const virtual_start, virtual_addr_t const virtual_end, physical_addr_t physical_base_) {
	u64 const start_index = virtual_start >> SECTION_SHIFT & (ENTRIES_PER_TABLE - 1);
	u64 const end_index = ((virtual_end >> SECTION_SHIFT) - 1) & (ENTRIES_PER_TABLE - 1);
	u64 const physical_base = physical_base_ & ~((1 << SECTION_SHIFT) - 1);
	for (u64 i = start_index; i <= end_index; ++i) {
		physical_addr_t const this_base = physical_base + (SECTION_SIZE * i);
		u64 const flags = this_base >= DEVICE_BASE ? TD_DEVICE_BLOCK_FLAGS : TD_KERNEL_BLOCK_FLAGS;
		pmd[i] = this_base | flags;
	}
}

static struct {
	table_t pgd;
	table_t pud;
	table_t pmds[NUM_PMDS];
} page_table __attribute__((aligned(PAGE_SIZE)));

void mmu_init(void) {
	create_table_entry(page_table.pgd, page_table.pud, 0, PGD_SHIFT, TD_KERNEL_TABLE_FLAGS);

	for (u64 i = 0; i < NUM_PMDS; ++i) {
		u64 const map_base = PUD_ENTRY_MAP_SIZE * i;
		create_table_entry(page_table.pud, page_table.pmds[i], map_base, PUD_SHIFT, TD_KERNEL_TABLE_FLAGS);

		u64 const offset = BLOCK_SIZE * i;
		create_block_map(page_table.pmds[i], offset, offset + BLOCK_SIZE, offset);
	}

	asm volatile("msr ttbr0_el1, %0" : : "r"(&page_table));
	asm volatile("msr tcr_el1, %0" : : "r"((u64)TCR_EL1_VALUE));
	asm volatile("msr mair_el1, %0" : : "r"((u64)MAIR_EL1_VALUE));

	{
		u64 sctlr;
		asm volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
		sctlr |= SCTLR_INSTRUCTIONS_CACHEABLE | SCTLR_DATA_CACHEABLE | SCTLR_MMU_ENABLED;
		asm volatile("msr sctlr_el1, %0" : : "r"(sctlr));
	}
}
