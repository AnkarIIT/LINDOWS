/*
 * LINDOWS GDT — Phase 2, step 1.
 * 32-bit protected-mode segmentation. We install our own GDT (rather than
 * trusting the bootloader's) so segment layout is under kernel control and we
 * can later extend it for a TSS (privilege rings) and per-CPU segments.
 *
 * Flat memory model: code + data both span all 4 GiB, base 0. No segmentation
 * is used for memory isolation — that comes from paging in a later step — but
 * a valid GDT is mandatory for 32-bit protected mode and for the far-jump
 * reload of CS.
 */
#ifndef LINDOWS_GDT_H
#define LINDOWS_GDT_H

#include "types.h"

/* Packed GDT descriptor: 8 bytes, see Intel SDM 3.4.5 / OSDev wiki. */
struct gdt_entry {
    u16 limit_low;    /* bits 0..15 of segment limit   */
    u16 base_low;     /* bits 0..15 of base address      */
    u8  base_middle;  /* bits 16..23 of base address     */
    u8  access;       /* present/dpl/type              */
    u8  granularity;  /* limit hi nibble | flags (G/D)  */
    u8  base_high;    /* bits 24..31 of base address     */
} __attribute__((packed));

/* GDTR: loaded by `lgdt`. */
struct gdt_ptr {
    u16 limit;
    u32 base;
} __attribute__((packed));

/* Build + install the GDT, then reload CS and the data-segment registers. */
void gdt_init(void);

#endif /* LINDOWS_GDT_H */
