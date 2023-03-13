// downloaded from https://github.com/raspberrypi/tools/blob/master/armstubs/armstub8.S
// see associated copyright notice

.equ LOCAL_CONTROL, 0xff800000
.equ LOCAL_PRESCALER, 0xff800008

.equ GIC_DISTB, 0xff841000
.equ GIC_CPUB, 0xff842000

.equ OSC_FREQ, 54000000

.equ SCR_RW, 1 << 10
.equ SCR_HCE, 1 << 8
.equ SCR_SMD, 1 << 7
.equ SCR_RES1_5, 1 << 5
.equ SCR_RES1_4, 1 << 4
.equ SCR_NS, 1 << 0
.equ SCR_VAL, SCR_RW | SCR_HCE | SCR_SMD | SCR_RES1_5 | SCR_RES1_4 | SCR_NS

.equ ACTLR_VAL, 1 << 0 | 1 << 1 | 1 << 4 | 1 << 5 | 1 << 6

.equ CPUECTLR_EL1_SMPEN, 1 << 6

.equ SPSR_EL3_D, 1 << 9
.equ SPSR_EL3_A, 1 << 8
.equ SPSR_EL3_I, 1 << 7
.equ SPSR_EL3_F, 1 << 6
.equ SPSR_EL3_MODE_EL2H, 9
.equ SPSR_EL3_VAL, SPSR_EL3_D | SPSR_EL3_A | SPSR_EL3_I | SPSR_EL3_F | SPSR_EL3_MODE_EL2H

.equ GICC_CTRLR, 0x0
.equ GICC_PMR, 0x4
.equ INTERRUPT_NUMBER, 0x8  // Number of interrupt enable registers (256 total irqs)
.equ GICD_CTRLR, 0x0
.equ GICD_IGROUPR, 0x80

.equ CPTR_ALLOW_SVE, 1 << 8
// also allows TFP with a 0 at bit 10
.equ CPTR_VAL, CPTR_ALLOW_SVE

.global _start
_start:
	// LOCAL_CONTROL:
	// Bit 9 clear: Increment by 1 (vs. 2).
	// Bit 8 clear: Timer source is 54MHz crystal (vs. APB).
	ldr x0, =LOCAL_CONTROL
	str wzr, [x0]
	// LOCAL_PRESCALER; divide-by (0x80000000 / register_val) == 1
	mov w1, 0x80000000
	str w1, [x0, #(LOCAL_PRESCALER - LOCAL_CONTROL)]

	// Set L2 read/write cache latency to 3
	mrs x0, S3_1_c11_c0_2
	mov x1, #0x22
	orr x0, x0, x1
	msr S3_1_c11_c0_2, x0

	// Set up CNTFRQ_EL0
	ldr x0, =OSC_FREQ
	msr CNTFRQ_EL0, x0

	// Set up CNTVOFF_EL2
	msr CNTVOFF_EL2, xzr

	// Enable FP/SIMD
	mov x0, #CPTR_VAL
	msr CPTR_EL3, x0

	// Set up SCR
	mov x0, #SCR_VAL
	msr SCR_EL3, x0

	// Set up ACTLR
	mov x0, #ACTLR_VAL
	msr ACTLR_EL3, x0

	// Set SMPEN
	mov x0, #CPUECTLR_EL1_SMPEN
	msr S3_1_C15_C2_1, x0

	// bl setup_gic

	// Set up SCTLR_EL2
	// All set bits below are res1. LE, no WXN/I/SA/C/A/M
	ldr x0, =0x30c50830
	msr SCTLR_EL2, x0

	// Switch to EL2
	mov x0, #SPSR_EL3_VAL
	msr spsr_el3, x0
	adr x0, in_el2
	msr elr_el3, x0
	eret
in_el2:

	mrs x6, MPIDR_EL1
	and x6, x6, #0x3
	cbz x6, primary_cpu

	adr x5, spin_cpu0
secondary_spin:
	wfe
	ldr x4, [x5, x6, lsl #3]
	cbz x4, secondary_spin
	mov x0, #0
	b boot_kernel

primary_cpu:
	ldr w4, kernel_entry32
	ldr w0, dtb_ptr32

boot_kernel:
	mov x1, #0
	mov x2, #0
	mov x3, #0
	br x4

.ltorg

.org 0xd8
.global spin_cpu0
spin_cpu0:
	.quad 0
.org 0xe0
.global spin_cpu1
spin_cpu1:
	.quad 0
.org 0xe8
.global spin_cpu2
spin_cpu2:
	.quad 0
.org 0xf0
.global spin_cpu3
spin_cpu3:
	# Shared with next two symbols/.word
	# FW clears the next 8 bytes after reading the initial value, leaving
	# the location suitable for use as spin_cpu3
.org 0xf0
.global stub_magic
stub_magic:
	.word 0x5afe570b
.org 0xf4
.global stub_version
stub_version:
	.word 0
.org 0xf8
.global dtb_ptr32
dtb_ptr32:
	.word 0x0
.org 0xfc
.global kernel_entry32
kernel_entry32:
	.word 0x0

.org 0x100

setup_gic:  // Called from secure mode - set all interrupts to group 1 and enable.
	mrs x0, MPIDR_EL1
	ldr x2, =GIC_DISTB
	tst x0, #0x3
	b.eq 2f  // primary core

	mov w0, #3  // Enable group 0 and 1 IRQs from distributor
	str w0, [x2, #GICD_CTRLR]
2:
	add x1, x2, #(GIC_CPUB - GIC_DISTB)
	mov w0, #0x1e7
	str w0, [x1, #GICC_CTRLR]  // Enable group 1 IRQs from CPU interface
	mov w0, #0xff
	str w0, [x1, #GICC_PMR]  // priority mask
	add x2, x2, #GICD_IGROUPR
	mov x0, #(INTERRUPT_NUMBER * 4)
	mov w1, #~0  // group 1 all the things
3:
	subs x0, x0, #4
	str w1, [x2, x0]
	b.ne 3b
	ret

.global dtb_space
dtb_space:
