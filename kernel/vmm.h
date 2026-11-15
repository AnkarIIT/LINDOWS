/*
 * LINDOWS VMM — virtual memory / paging (32-bit, 4 KiB pages).
 *
 * Layout established at boot (linear = physical until the higher-half
 * relocation, which comes later):
 *
 *   0x00000000..RAM     identity map of all tracked RAM (supervisor RW)
 *   0xC0000000..        kernel alias (first 1 MiB of the kernel image)
 *   0xFFFFF000          recursive page directory (PD maps itself)
 *
 * Page tables are allocated from the PMM and written before PG is enabled
 * (linear == physical pre-PG, so writing at the physical address works).
 * After PG is on, the identity map covers all tracked RAM, so table
 * maintenance keeps working at linear==physical addresses.
 */
#ifndef LINDOWS_VMM_H
#define LINDOWS_VMM_H

#include "types.h"

#define VMM_KALIAS_BASE 0xC0000000u   /* kernel alias window            */
#define VMM_RECURSE     0xFFFFF000u   /* recursive page-directory slot  */

/* Build tables from PMM state and enable paging (CR3 + CR0.PG + CR0.WP). */
void vmm_init(void);

/* Functional self-check: alias read matches direct read, and interrupts
 * (PIT) still flow with PG on. Panics on failure. */
void vmm_verify(void);

/* Physical backing of a mapped virtual address, or 0 if not present. */
u32 vmm_get_phys(u32 virt);

/* Frames the identity map covers (== pmm_installed_frames()). */
u32 vmm_identity_frames(void);

#endif /* LINDOWS_VMM_H */
