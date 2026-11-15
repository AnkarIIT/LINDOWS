/*
 * LINDOWS VMM implementation — see vmm.h for the design.
 * All table maintenance goes through the recursive mapping.
 */
#include "vmm.h"
#include "pmm.h"
#include "console.h"
#include "panic.h"

#define PTE_P  0x001u    /* present  */
#define PTE_RW 0x002u    /* writable */

/* boot.S's early tables (linker symbols, VMA; phys = vma - VMM_V2P). */
extern const u8 early_pd[];
extern const u8 early_low_pt[];

static u32* page_directory = 0;   /* VMA view of the PD (kernel image) */
static u32  pd_phys        = 0;
static u32  identity_frames = 0;

/* Recursive views: the PD itself at 0xFFFFF000; PT for PD index i at
 * 0xFFC00000 + i*4 KiB. Valid as soon as PD[1023] = pd_phys | P|RW. */
static volatile u32* pd_view(void) {
    return (volatile u32*)VMM_RECURSE;
}
static volatile u32* pt_view(u32 pdi) {
    return (volatile u32*)(0xFFC00000u + pdi * PAGE_SIZE);
}

void vmm_init(void) {
    identity_frames = pmm_installed_frames();
    page_directory  = (u32*)early_pd;
    pd_phys         = (u32)early_pd - VMM_V2P;

    /* boot.S already installed:
     *   PD[0] = PD[768] = early_low_pt  (identity 0..4 MiB + kernel window)
     *   PD[1023] = pd_phys | P|RW       (recursive)
     * Continue the identity map for the remaining RAM: one PT per 4 MiB. */
    for (u32 base = 1024; base < identity_frames; base += 1024) {
        u32 pdi = base / 1024;
        u32 pt_phys = pmm_alloc();
        if (!pt_phys)
            panic("vmm: out of frames while building page tables", 0, 0);
        pd_view()[pdi] = pt_phys | PTE_P | PTE_RW;      /* link, then fill */
        volatile u32* pt = pt_view(pdi);
        u32 n = (identity_frames - base >= 1024) ? 1024 : identity_frames - base;
        for (u32 i = 0; i < n; i++)
            pt[i] = ((base + i) << 12) | PTE_P | PTE_RW;
    }

    /* Reload CR3 so the new PD entries are authoritative. PG/WP are already
     * on (boot.S); this just flushes stale TLB state. */
    __asm__ volatile("mov %0, %%cr3" :: "r"(pd_phys) : "memory");
}

u32 vmm_get_phys(u32 virt) {
    u32 pdi = virt >> 22;
    u32 pti = (virt >> 12) & 0x3FF;
    if (!(pd_view()[pdi] & PTE_P)) return 0;
    volatile u32* pt = pt_view(pdi);
    if (!(pt[pti] & PTE_P)) return 0;
    return (pt[pti] & 0xFFFFF000u) | (virt & 0xFFFu);
}

int vmm_map_page(u32 virt, u32 phys) {
    if ((virt | phys) & 0xFFFu) return -1;   /* both must be page-aligned */
    u32 pdi = virt >> 22;
    u32 pti = (virt >> 12) & 0x3FF;

    if (!(pd_view()[pdi] & PTE_P)) {
        /* No page table covers this 4 MiB yet: take a frame for one, link
         * it into the PD, and zero it through the recursive view (same
         * link-then-fill order as vmm_init). Nothing else can be using this
         * 4 MiB region, so the transient unzeroed window is safe. */
        u32 pt_phys = pmm_alloc();
        if (!pt_phys) return -1;
        pd_view()[pdi] = pt_phys | PTE_P | PTE_RW;
        volatile u32* pt = pt_view(pdi);
        for (u32 i = 0; i < 1024; i++) pt[i] = 0;
    }

    volatile u32* pt = pt_view(pdi);
    pt[pti] = phys | PTE_P | PTE_RW;
    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
    return 0;
}

u32 vmm_unmap_page(u32 virt) {
    u32 pdi = virt >> 22;
    u32 pti = (virt >> 12) & 0x3FF;
    if (!(pd_view()[pdi] & PTE_P)) return 0;
    volatile u32* pt = pt_view(pdi);
    u32 old = pt[pti];
    if (!(old & PTE_P)) return 0;
    pt[pti] = 0;
    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
    return old & 0xFFFFF000u;
}

void vmm_verify(void) {
    /* 1. Kernel VMA <-> identity alias: page_directory lives in the kernel
     *    image, so phys = vma - V2P; reading at that linear address must
     *    return the same bytes as the direct VMA read. */
    volatile const u32* via_identity = (const u32*)((u32)page_directory - VMM_V2P);
    if (via_identity[0]     != page_directory[0] ||
        via_identity[768]   != page_directory[768] ||
        via_identity[1023]  != page_directory[1023])
        panic("vmm: identity<->kernel alias mismatch", (u32)page_directory, 0);

    /* 2. Recursive PD view must mirror the direct view. */
    if (pd_view()[0]     != page_directory[0] ||
        pd_view()[768]   != page_directory[768] ||
        pd_view()[1023]  != page_directory[1023])
        panic("vmm: recursive PD readback mismatch", VMM_RECURSE, 0);

    /* 3. Interrupts must still flow with paging on: wait for PIT ticks. */
    extern volatile u32 pit_ticks;
    u32 t0 = pit_ticks;
    u32 spins = 0;
    while (pit_ticks < t0 + 3) {
        __asm__ volatile("hlt");
        if (++spins > 100000000u)
            panic("vmm: PIT dead with paging enabled", 0, 0);
    }
}

u32 vmm_identity_frames(void) { return identity_frames; }
