/*
 * LINDOWS Phase 3 — minimal x86_64 linker script
 * Output: ELF64 kernel at higher-half VMA = 0xffffffff80000000
 * Entry point: kmain64 (defined in kernel/main64.c)
 * .limine_requests section must be contiguous and KEEP'd.
 */

OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kmain)

SECTIONS
{
    /* Set the VMA (virtual address) where the kernel will be placed.
     * The Limine loader maps this higher-half region. */
    . = 0xffffffff80000000;

    /* .text section — contains the kernel code */
    .text : {
        *(.text*)
    }

    /* .rodata section — read-only data */
    .rodata : {
        *(.rodata*)
    }

    /* .data section — initialized data */
    .data : {
        *(.data*)
    }

    /* .bss section — uninitialized data (zero-filled at startup) */
    . = ALIGN(4096);
    .bss : {
        *(.bss*)
        *(.bss.*)
    }

    /* .limine_requests section — must be at the VMA base.
     * The Limine loader uses start/end markers to locate it. */
    .limine_requests : {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    }

    /* Align to page boundary */
    . = ALIGN(4096);
}