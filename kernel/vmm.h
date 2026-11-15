/*
 * LINDOWS VMM — virtual memory / paging (32-bit, 4 KiB pages).
 *
 * The kernel is higher-half: linked at 0xC0101000 (VMA = phys LMA + 0xC0000000),
 * at physical 1 MiB; phys = vma - VMM_V2P inside the kernel image.
 *
 * Layout after boot.S + vmm_init():
 *
 *   0x00000000..RAM      identity map of all tracked RAM (supervisor RW)
 *   0xC0000000..0xC0400000  kernel window (same frames as the identity map —
 *                        the kernel's VMA lives at 0xC0100000 = phys 1 MiB)
 *   0xFFFFF000           recursive page directory (PD maps itself)
 *
 * boot.S builds the early tables (early_pd / early_low_pt, reserved in .bss):
 * PD[0] = PD[768] = early_low_pt (identity 0..4 MiB + kernel window),
 * PD[1023] = early_pd (recursive). vmm_init() adopts that PD and extends it.
 *
 * Table maintenance goes through the RECURSIVE mapping (0xFFFFF000 for the
 * PD, 0xFFC00000+ for page tables), so a page table can be filled no matter
 * where its frame physically sits — physical addresses are no longer
 * readable directly once the kernel lives above 3 GiB.
 */
#ifndef LINDOWS_VMM_H
#define LINDOWS_VMM_H

#include "types.h"

#define VMM_V2P          0xC0000000u   /* vma - V2P = phys (kernel image)  */
#define VMM_KERNEL_VIRT  0xC0101000u   /* kernel base VMA (must equal what
                                            linker.ld computes: first 4 KiB
                                            page after the stub, + 0xC0000000) */
#define VMM_RECURSE      0xFFFFF000u   /* recursive page-directory slot    */

/* Adopt boot.S's early tables, map all tracked RAM, reload CR3. */
void vmm_init(void);

/* Map one page-aligned (virt, phys) pair, RW + present, supervisor. Creates
 * the covering page table on demand (a frame is taken from the PMM for it).
 * Returns 0 on success, -1 on bad alignment or frame exhaustion. */
int vmm_map_page(u32 virt, u32 phys);

/* Unmap a page. Returns the physical frame that was mapped (0 if none);
 * the caller owns it — e.g. hand it back to pmm_free(). */
u32 vmm_unmap_page(u32 virt);

/* Functional self-check: identity<->kernel alias, recursive PD readback,
 * and PIT ticks with paging on. Panics on failure. */
void vmm_verify(void);

/* Physical backing of a mapped virtual address, or 0 if not present. */
u32 vmm_get_phys(u32 virt);

/* Frames the identity map covers (== pmm_installed_frames()). */
u32 vmm_identity_frames(void);

#endif /* LINDOWS_VMM_H */
