/*
 * LINDOWS heap — kernel dynamic memory (Phase 2).
 *
 * First-fit free-list allocator with split + coalesce, grown on demand:
 * pages come from the PMM and are mapped into a dedicated region via the
 * VMM.
 *
 * Heap home: 0xC8000000 (PD index 800) — its own region ABOVE the kernel
 * window (0xC0000000..0xC0400000) and far above the image, so the heap's
 * page tables are freshly allocated by vmm_map_page() and the identity map
 * plus kernel alias stay byte-pure. Capped at 4 MiB (one PD entry's worth:
 * 0xC8000000..0xC8400000) — plenty for Phase 2, cheap to raise later.
 *
 * Block layout (every block starts 8-byte aligned, blocks are contiguous):
 *
 *   +16 header | payload ...      used block (kmalloc'd pointer = +16)
 *   +16 header | payload ...      free block (adjacent free ones coalesce)
 *
 * The header is 16 bytes: {u32 size; u32 used; next; prev;}. `size` counts
 * payload bytes only. kmalloc(0) is NULL by policy; kfree(NULL) is a no-op.
 *
 * Single-core, IRQs can fire but the allocator has no other users yet —
 * a spinlock arrives with the scheduler.
 */
#ifndef LINDOWS_HEAP_H
#define LINDOWS_HEAP_H

#include "types.h"

#define HEAP_BASE 0xC8000000u
#define HEAP_MAX  0x00400000u   /* 4 MiB: 0xC8000000..0xC8400000 */

/* Call once, after pmm_init() + vmm_init(). Starts with zero pages mapped;
 * the first allocation maps them. */
void heap_init(void);

/* kmalloc: 8-byte aligned payload, NULL on 0 bytes or exhaustion.
 * kfree: no-op on NULL, panics on a pointer outside the heap, misaligned
 * pointer, or a corrupt/double-freed block header.
 * kcalloc: kmalloc + zero fill (overflow-checked n*m).
 * krealloc: NULL p = kmalloc(n); n = 0 = kfree(p) + NULL; else
 * alloc-copy-free preserving min(old,new) bytes. */
void*  kmalloc(u32 n);
void   kfree(void* p);
void*  kcalloc(u32 n, u32 m);
void*  krealloc(void* p, u32 n);

/* Live self-check (pattern write/readback, coalescing, stats hygiene).
 * Panics with details on the first failure. */
void heap_verify(void);

/* Diagnostics for the boot banner / smoke test. */
u32 heap_pages_mapped(void);
u32 heap_allocs(void);
u32 heap_frees(void);
u32 heap_peak_bytes(void);

#endif /* LINDOWS_HEAP_H */
