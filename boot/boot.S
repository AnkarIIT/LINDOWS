/*
 * LINDOWS boot stub — GNU as syntax.
 * Multiboot (v1) header so GRUB can load us in 32-bit protected mode, plus the
 * hand-written entry point that calls kmain. Phase 1 bootstrap.
 */
.code32

.section .multiboot
.include "multiboot.inc"          /* constants — found via -Iboot in ASFLAGS */

.section .text
.global _start
.extern kmain

/* Multiboot v1 header: must live within the first 8 KiB of the binary. */
multiboot_header:
    .long MULTIBOOT_HEADER_MAGIC
    .long MULTIBOOT_HEADER_FLAGS
    .long MULTIBOOT_HEADER_CHECKSUM

_start:
    cli                           /* no interrupts until the IDT is up */
    mov $stack_top, %esp          /* Multiboot v1: loader regs undefined, don't trust %esp */
    xor %ebp, %ebp                /* terminate backtrace chain */
    call kmain
hang:
    hlt
    jmp hang
