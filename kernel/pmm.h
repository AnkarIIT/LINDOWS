/*
 * LINDOWS PMM — physical (frame) memory manager.
 *
 * Backed by the multiboot memory map (saved by boot.S into mb_info).
 * A bitmap of 4 KiB frames, capped at PMM_MAX_FRAMES (1 GiB) to bound BSS.
 * Frames below 1 MiB are permanently "used" (BIOS/EBDA/VGA/low mem).
 * The kernel image region [kstart, kend) is marked used, which also covers
 * the bitmap itself (it lives in .bss).
 *
 * Single-core, IRQs can fire but there are no other PMM users yet — locking
 * comes with the scheduler (Phase 2/3).
 */
#ifndef LINDOWS_PMM_H
#define LINDOWS_PMM_H

#include "types.h"

#define PAGE_SIZE 4096u

/* Initialize from a multiboot info pointer. Call once, before paging. */
void pmm_init(u32 mb_info);

/* Allocate one frame. Returns its physical address, or 0 if exhausted
 * (frame 0 is always "used", so 0 is unambiguous). */
u32 pmm_alloc(void);

/* Return a frame to the free pool. */
void pmm_free(u32 phys);

/* Frames of installed RAM tracked (capped), and currently free frames. */
u32 pmm_installed_frames(void);
u32 pmm_free_frames(void);

/* 1 if the memory map was used, 0 if we fell back to mem_upper. */
u32 pmm_used_mmap(void);

#endif /* LINDOWS_PMM_H */
