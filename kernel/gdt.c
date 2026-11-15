/*
 * LINDOWS GDT — Phase 2, step 1.
 * Three descriptors: null, code (0x08), data (0x10). The selector values
 * (0x08 / 0x10) are the conventional indexes and are used by the far jump
 * below to reload CS and by the data-segment register reloads.
 */
#include "gdt.h"

/* Number of descriptors; the null entry occupies index 0. */
#define GDT_ENTRIES 3

/* GDT in BSS-backed static storage so it survives after gdt_init returns. */
static struct gdt_entry gdt_entries[GDT_ENTRIES];
static struct gdt_ptr   gdt_register;

/*
 * A single 8-byte descriptor, built from base/limit/access/granularity.
 * The granularity nibble encodes limit bits 16..19 (page granularity flag G
 * and 32-bit flag D live in the high nibble).
 */
static struct gdt_entry make_descriptor(u32 base, u32 limit, u8 access, u8 gran) {
    struct gdt_entry e;
    e.limit_low     = (u16)(limit & 0xFFFF);
    e.base_low      = (u16)(base & 0xFFFF);
    e.base_middle   = (u8)((base >> 16) & 0xFF);
    e.access        = access;
    e.granularity   = (u8)(((limit >> 16) & 0x0F) | (gran & 0xF0));
    e.base_high     = (u8)((base >> 24) & 0xFF);
    return e;
}

/*
 * Reload CS via a far jump to selector 0x08 (our code descriptor) and reload
 * the data-segment registers to 0x10 (our data descriptor). The `1:` local
 * label is the post-jump target; GAS resolves `$1f` as a forward local label.
 * gdt_base/gdt_limit come straight from gdt_register; we keep the pointer in
 * a stack-local so `lgdt` reads it before the far jump clobbers nothing.
 */
static void gdt_reload(u32 gdt_base, u16 gdt_limit) {
    struct { u16 limit; u32 base; } __attribute__((packed)) gdtr = { gdt_limit, gdt_base };
    __asm__ volatile(
        "lgdt %0\n\t"
        "ljmp $0x08, $1f\n\t"      /* CS <- code descriptor; flush pipeline */
        "1:\n\t"
        "mov  $0x10, %%eax\n\t"      /* data descriptor */
        "mov  %%ax, %%ds\n\t"
        "mov  %%ax, %%es\n\t"
        "mov  %%ax, %%fs\n\t"
        "mov  %%ax, %%gs\n\t"
        "mov  %%ax, %%ss"
        :
        : "m"(gdtr)
        : "eax", "memory");
}

void gdt_init(void) {
    /*
     * Access bytes (Intel SDM 3.4.5.1):
     *   code 0x9A = P(1) DPL(00) S(1) Type(1010)  exec/read, accessed
     *   data 0x92 = P(1) DPL(00) S(1) Type(0010)  data, read/write, accessed
     * Granularity byte: G(1)|D(1)|0|0 = 0xC0 -> 4KiB pages, 32-bit.
     */
    gdt_entries[0] = make_descriptor(0, 0, 0, 0);                 /* null      */
    gdt_entries[1] = make_descriptor(0, 0x000FFFFF, 0x9A, 0xC0); /* code 0x08 */
    gdt_entries[2] = make_descriptor(0, 0x000FFFFF, 0x92, 0xC0); /* data 0x10 */

    gdt_register.limit = (sizeof(gdt_entries) - 1);              /* 23 */
    gdt_register.base  = (u32)&gdt_entries;

    gdt_reload(gdt_register.base, gdt_register.limit);
}
