.section ".text.boot"

.global _start
.extern standard_init
.extern main

_start:
	// start stack below our code
	ldr x4, =_start
	mov sp, x4

	ldr x4, =__bss_start
	ldr w5, =__bss_size
.loop:
	cbz w5, .endloop
	str xzr, [x4], #8
	subs w5, w5, #1
	// != 0
	bne .loop
.endloop:

	bl standard_init
	bl main

.halt:
	wfe
	b .halt
