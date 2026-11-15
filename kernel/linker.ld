/* linker.ld — LINDOWS kernel linker script.
 *
 * The kernel is a HIGHER-HALF kernel:
 *   - loaded by the multiboot loader at PHYSICAL ~1 MiB (LMA),
 *   - linked/running at VIRTUAL LMA + 0xC0000000 (VMA; 0xC0101000 today),
 *   - phys = vma - 0xC0000000 for everything inside the kernel image.
 *
 * boot/boot.S runs pre-paging out of .stub (VMA == LMA == phys 1 MiB),
 * builds the early page tables (early_pd / early_low_pt, reserved in .bss),
 * enables paging, then calls kmain at its higher-half VMA.
 *
 * The multiboot v1 header must be within the first 8 KiB of the loaded
 * image — it is the first thing in .stub at LMA 0x100000.
 */

ENTRY(_start)

KERNEL_PHYS = 0x00100000;
KERNEL_VIRT = 0xC0000000;
V2P         = 0xC0000000;

PHDRS {
    stub   PT_LOAD FLAGS(5);   /* R E — pre-paging stub, runs at phys */
    text   PT_LOAD FLAGS(5);   /* R E */
    rodata PT_LOAD FLAGS(4);   /* R   */
    data   PT_LOAD FLAGS(6);   /* RW  */
}

SECTIONS
{
    /* ---- pre-paging stub: VMA == LMA == physical 1 MiB ---- */
    . = KERNEL_PHYS;

    .stub : {
        *(.multiboot)          /* header must be first (within 8 KiB)   */
        *(.stub .stub.*)
    } :stub

    /* The kernel image starts at the first 4 KiB page AFTER the stub, and its
     * VMA is that LMA + 0xC0000000, so that LMA = VMA - 0xC0000000 holds
     * EXACTLY for every kernel byte (boot.S and the VMM rely on this uniform
     * delta; the 0xC0000000 window alias is then also byte-accurate). */
    KERNEL_LMA = ALIGN(LOADADDR(.stub) + SIZEOF(.stub), 4096);
    KERNEL_VIRT = KERNEL_LMA + V2P;

    . = KERNEL_VIRT;

    _kstart = .;

    /* Every kernel section: LMA = VMA - V2P, derived FROM the section's own
     * address. This keeps the phys<->virt delta exactly V2P even where ld
     * inserts alignment padding between sections (a hand-chained
     * LOADADDR(prev)+SIZEOF(prev) drifts out of sync with the aligned VMA). */
    .text : AT(ADDR(.text) - V2P) {
        *(.text .text.*)
    } :text

    .rodata : AT(ADDR(.rodata) - V2P) {
        *(.rodata .rodata.*)
    } :rodata

    .data : AT(ADDR(.data) - V2P) {
        *(.data .data.*)
    } :data

    .bss : AT(ADDR(.bss) - V2P) {
        __bss_start = .;
        *(COMMON)
        *(.bss .bss.*)

        /* Early page tables used by boot.S before the VMM takes over.
         * NOBITS: the loader zero-fills them at their physical address
         * (LMA region), and boot.S touches them by phys = vma - V2P. */
        . = ALIGN(4096);
        early_pd     = .;   . += 4096;
        early_low_pt = .;   . += 4096;

        /* Kernel stack: multiboot v1 leaves %esp undefined, so we install
         * our own (VMA) after the jump into the higher half. */
        . = ALIGN(16);
        stack_bottom = .;
        . += 16384;
        stack_top = .;
        __bss_end = .;
    } :data

    _kend = .;

    /* Physical (LMA) counterparts of the kernel image bounds. */
    _kphys_start = _kstart - V2P;
    _kphys_end   = _kend   - V2P;
}
