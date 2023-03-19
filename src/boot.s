.section ".text.boot"

// LE, no WXN/I/SA/C/A/M.
.equ SCTLR_EL2_RESERVED, 3 << 4 | 1 << 11 | 1 << 16 | 1 << 18 | 3 << 22 | 3 << 28

.equ SCTLR_EL2_VALUE, SCTLR_EL2_RESERVED

// RES1 bits should be set to 1.
.equ SCTLR_EL1_RESERVED, (3 << 28) | (3 << 22) | (1 << 20) | (1 << 11)
// Data accesses at EL1 are little-endian.
.equ SCTLR_EL1_EE_LITTLE_ENDIAN, 0 << 25
// Data accesses at EL0 are little-endian.
.equ SCTLR_EL1_EOE_LITTLE_ENDIAN, 0 << 24
// Instruction accesses are not cacheable.
.equ SCTLR_EL1_I_CACHE_DISABLED, 0 << 12
// Data accesses are not cacheable.
.equ SCTLR_EL1_D_CACHE_DISABLED, 0 << 2
// MMU is not enabled.
.equ SCTLR_EL1_MMU_DISABLED, 0 << 0

.equ SCTLR_EL1_VALUE, SCTLR_EL1_RESERVED | SCTLR_EL1_EE_LITTLE_ENDIAN | SCTLR_EL1_I_CACHE_DISABLED | SCTLR_EL1_D_CACHE_DISABLED | SCTLR_EL1_MMU_DISABLED

// Levels below EL2 will use AArch64.
.equ HCR_AARCH64, 1 << 31

.equ HCR_VALUE, HCR_AARCH64

.equ SPSR_MASK_ALL, 0b1111 << 6
.equ SPSR_RETURN_TO_EL1h, 0b0101
.equ SPSR_RETURN_TO_EL2h, 0b1001

.equ SPSR_VALUE, SPSR_MASK_ALL | SPSR_RETURN_TO_EL1h

.equ CPACR_ENABLE_FLOATING_POINT, 3 << 20
.equ CPACR_ENABLE_SVE, 3 << 16

.equ CPACR_VALUE, CPACR_ENABLE_FLOATING_POINT | CPACR_ENABLE_SVE

.equ CPTR_ALLOW_SVE, 0b11 << 16
.equ CPTR_VALUE, CPTR_ALLOW_SVE

.equ SPSR_EL3_D, 1 << 9
.equ SPSR_EL3_A, 1 << 8
.equ SPSR_EL3_I, 1 << 7
.equ SPSR_EL3_F, 1 << 6
.equ SPSR_EL3_MODE_EL2H, 9
.equ SPSR_EL3_VAL, SPSR_EL3_D | SPSR_EL3_A | SPSR_EL3_I | SPSR_EL3_F | SPSR_EL3_MODE_EL2H

.global _start
.extern standard_init
.extern main

_start:
	// The armstub has already ensured that only core 0 gets here.

	// Switch to EL2.
	// Do it here instead of in the armstub so we can set the exception vector table.
	adr x0, exception_vector_table
	msr vbar_el3, x0

	ldr x0, =SCTLR_EL2_VALUE
	msr SCTLR_EL2, x0

	mov x0, #SPSR_EL3_VAL
	msr spsr_el3, x0

	adr x0, .in_el2
	msr elr_el3, x0
	eret

.in_el2:

	// Switch to EL1.
	ldr x4, =SCTLR_EL1_VALUE
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

	adr x4, exception_vector_table
	msr vbar_el2, x4

	eret

.in_el1:

	// Start the stack below our code.
	ldr x4, =_start
	mov sp, x4

	// Zero BSS.
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
