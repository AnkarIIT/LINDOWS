/*
 * LINDOWS GDT — minimal Phase 2, step 1.
 * Three descriptors: null, code (0x08), data (0x10).
 * The far jump below reloads CS after lgdt; data registers are reloaded
 * by the caller (or by the Limine loader in Phase 3).
 */

#include "types.h"

/* GDT descriptor table (3 entries).  Base/limit set in gdt_init(). */
struct gdt_entry {
    u16 limit_low;
    u16 base_low;
    u8  base_middle;
    u8  access;
    u8  granularity;
    u8  base_high;
};

/* GDT pointer: limit (bytes - 1) + base address. */
struct gdt_ptr {
    u16 limit;
    u32 base;
};

/* GDT entries and pointer (static so they survive after gdt_init returns). */
static struct gdt_entry gdt_entries[3];
static struct gdt_ptr   gdt_register;

/* Build a GDT entry from base/limit/access/granularity. */
static void make_descriptor(struct gdt_entry *e, u32 base, u32 limit, u8 access, u8 gran) {
    e->limit_low     = (u16)(limit & 0xFFFF);
    e->base_low      = (u16)(base & 0xFFFF);
    e->base_middle   = (u8)((base >> 16) & 0xFF);
    e->access        = access;
    e->granularity   = (u8)(((limit >> 16) & 0x0F) | (gran & 0xF0));
    e->base_high     = (u8)((base >> 24) & 0xFF);
}

/* Reload GDT.  The lgdt flushes the pipeline; data registers
 * are reloaded by the caller (or the Limine loader in Phase 3). */
static void gdt_reload(u32 gdt_base, u16 gdt_limit) {
    struct { u16 limit; u32 base; } __attribute__((packed)) gdtr = { gdt_limit, gdt_base };
    __asm__ volatile(
        "lgdt %0\n\t"
        : /* no output */
        : "m"(gdtr)
        : /* clobbers: none needed since we removed ljmp */
    );
}

/* Initialise the GDT and reload segment registers. */
void gdt_init(void) {
    /* GDT entries: null, code 0x08, data 0x10 */
    make_descriptor(&gdt_entries[0], 0, 0, 0, 0);                 /* null      */
    make_descriptor(&gdt_entries[1], 0, 0x000FFFFF, 0x9A, 0xC0); /* code 0x08 */
    make_descriptor(&gdt_entries[2], 0, 0x000FFFFF, 0x92, 0xC0); /* data 0x10 */

    gdt_register.limit = (u16)(sizeof(gdt_entries) - 1);          /* 23      */
    gdt_register.base  = (u32)gdt_entries;                       /* BSS addr*/

    gdt_reload(gdt_register.base, gdt_register.limit);
}