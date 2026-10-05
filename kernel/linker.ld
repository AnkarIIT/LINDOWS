/* linker.ld — stage-1 linker script for the 32-bit LINDOWS kernel.
 * Loaded by GRUB at 1MiB; symbols exported so the kernel can find itself. */

ENTRY(_start)

SECTIONS
{
    /* GRUB loads the kernel at 1 MiB (0x00100000). */
    . = 0x00100000;

    _kstart = .;

    .text : {
        *(.multiboot)        /* multiboot header must be in first 8KiB */
        *(.text .text.*)
    }

    .rodata : {
        *(.rodata .rodata.*)
    }

    .data : {
        *(.data .data.*)
    }

    .bss : {
        __bss_start = .;
        *(COMMON)
        *(.bss .bss.*)
        __bss_end = .;
    }

    _kend = .;
}
