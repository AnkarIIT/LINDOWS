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
        /* Kernel stack. Multiboot v1 leaves registers undefined, so we cannot
           trust the loader's %esp. Reserve 16 KiB and install it in _start. */
        . = ALIGN(16);
        stack_bottom = .;
        . += 16384;
        stack_top = .;
        __bss_end = .;
    }

    _kend = .;
}
