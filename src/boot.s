.section ".text.boot"

// RES1 bits should be set to 1
.equ SCTLR_RESERVED, (3 << 28) | (3 << 22) | (1 << 20) | (1 << 11)
// data accesses at EL1 are little-endian
.equ SCTLR_EE_LITTLE_ENDIAN, 0 << 25
// data accesses at EL0 are little-endian
.equ SCTLR_EOE_LITTLE_ENDIAN, 0 << 24
// instruction accesses are not cacheable
.equ SCTLR_I_CACHE_DISABLED, 0 << 12
// data accesses are not cacheable
.equ SCTLR_D_CACHE_DISABLED, 0 << 2
// MMU is not enabled
.equ SCTLR_MMU_DISABLED, 0 << 0

.equ SCTLR_VALUE, SCTLR_RESERVED | SCTLR_EE_LITTLE_ENDIAN | SCTLR_I_CACHE_DISABLED | SCTLR_D_CACHE_DISABLED | SCTLR_MMU_DISABLED

// levels below EL2 will use AArch64
.equ HCR_AARCH64, 1 << 31

.equ HCR_VALUE, HCR_AARCH64

// mask all of DAIF
.equ SPSR_MASK_ALL, 0b1111 << 6
// return to EL1 non-thumb
.equ SPSR_RETURN_TO_EL1h, 0b0101
// return to EL2 non-thumb
.equ SPSR_RETURN_TO_EL2h, 0b1001

.equ SPSR_VALUE, SPSR_MASK_ALL | SPSR_RETURN_TO_EL1h

// don't trap floating point operations
.equ CPACR_ENABLE_FLOATING_POINT, 3 << 20
// don't trap SVE operations
.equ CPACR_ENABLE_SVE, 3 << 16

.equ CPACR_VALUE, CPACR_ENABLE_FLOATING_POINT | CPACR_ENABLE_SVE

.equ CPTR_ALLOW_SVE, 0b11 << 16
.equ CPTR_VALUE, CPTR_ALLOW_SVE

.global _start
.extern standard_init
.extern main

_start:
	// armstub has already ensured that only core 0 gets here

	// switch to EL1

	ldr x4, =SCTLR_VALUE
	msr sctlr_el1, x4

	mov x4, #HCR_VALUE
	msr hcr_el2, x4

	mov x4, #CPTR_VALUE
	msr cptr_el2, x4

	mov x4, #CPACR_VALUE
	msr cpacr_el1, x4

	mov x4, #SPSR_VALUE
	msr spsr_el2, x4

	adr x4, .in_el1
	msr elr_el2, x4

	eret

.in_el1:

	// start stack below our code

	ldr x4, =_start
	mov sp, x4

	// zero BSS

	ldr x4, =__bss_start
	ldr w5, =__bss_size
.loop:
	cbz w5, .endloop
	str xzr, [x4], #8
	subs w5, w5, #1
	// != 0
	bne .loop
.endloop:

	bl mmu_init

	bl standard_init
	bl main
	b halt
