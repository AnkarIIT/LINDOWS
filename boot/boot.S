/*
 * LINDOWS boot stub — GNU as syntax.
 * Multiboot (v1) header so GRUB can load us in 32-bit protected mode, plus the
 * hand-written entry point that calls kmain. Phase 1 bootstrap.
 *
 * Multiboot v1 contract: on entry EAX = 0x2BADB002 (magic) and
 * EBX = pointer to the multiboot info struct. Both are passed to kmain; EBX is
 * the memory map source that Phase-2 paging will need.
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
    cmp $0x2BADB002, %eax         /* did a multiboot loader actually start us? */
    jne bad_magic
    cli                           /* no interrupts until the IDT is up */
    mov $stack_top, %esp          /* Multiboot v1: loader regs undefined, don't trust %esp */
    xor %ebp, %ebp                /* terminate backtrace chain */
    push %ebx                     /* arg2: multiboot info pointer (saved!) */
    push %eax                     /* arg1: magic */
    call kmain
hang:
    hlt
    jmp hang

bad_magic:
    /* Not loaded by a multiboot loader — blink a red 'X' top-left and stop. */
    mov $0xB8000, %edi
    mov $0x4F58, %ax              /* 'X', red on black */
    stosw
    cli
1:  hlt
    jmp 1b
