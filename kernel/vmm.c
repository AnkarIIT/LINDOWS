/*
 * LINDOWS VMM implementation — see vmm.h for the design.
 */
#include "vmm.h"
#include "pmm.h"
#include "console.h"
#include "panic.h"

#define PTE_P 0x001u    /* present                     */
#define PTE_RW 0x002u   /* writable                    */

/* Kernel image base (linker symbol). */
extern const u8 _kstart[];

static u32 page_directory[1024] __attribute__((aligned(4096)));
static u32 identity_frames = 0;

static void load_cr3(u32 phys) {
    __asm__ volatile("mov %0, %%cr3" :: "r"(phys) : "memory");
}

static u32 make_pt(void) {
    u32 phys = pmm_alloc();
    if (!phys)
        panic("vmm: out of frames while building page tables", 0, 0);
    u32* pt = (u32*)phys;               /* linear == physical pre-PG */
    for (u32 i = 0; i < 1024; i++) pt[i] = 0;
    return phys;
}

void vmm_init(void) {
    identity_frames = pmm_installed_frames();

    /* 1. Identity-map all tracked RAM. `base` counts frames: 1024 frames
     *    (4 MiB) per page table -> PD index = base/1024. */
    for (u32 base = 0; base < identity_frames; base += 1024) {
        u32 pt_phys = make_pt();
        u32* pt = (u32*)pt_phys;
        u32 n = (identity_frames - base >= 1024) ? 1024 : identity_frames - base;
        for (u32 i = 0; i < n; i++)
            pt[i] = ((base + i) << 12) | PTE_P | PTE_RW;
        page_directory[base / 1024] = pt_phys | PTE_P | PTE_RW;
    }

    /* 2. Kernel alias: linear 0xC0000000..0xC0100000 -> phys [_kstart..+1MiB).
     *    NOTE: kernel must stay under 1 MiB for this mapping to be complete. */
    u32 kbase = (u32)_kstart;
    if (kbase & 0xFFE00000u)
        panic("vmm: kernel base not 2MiB-aligned under 0xC0000000 window", 0, 0);
    u32 alias_pt = make_pt();
    u32* apt = (u32*)alias_pt;
    for (u32 i = 0; i < 256; i++)
        apt[i] = (kbase + i * PAGE_SIZE) | PTE_P | PTE_RW;
    page_directory[0x300] = alias_pt | PTE_P | PTE_RW;   /* 0xC0000000/4MiB */

    /* 3. Recursive mapping: last PD entry points at the PD itself. */
    page_directory[1023] = (u32)page_directory | PTE_P | PTE_RW;

    /* 4. Enable paging. PG (bit 31) + WP (bit 16: write-protect RO pages). */
    load_cr3((u32)page_directory);
    u32 cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80010000u;
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0) : "memory");
}

u32 vmm_get_phys(u32 virt) {
    u32 pdi = virt >> 22;
    u32 pte_idx = (virt >> 12) & 0x3FF;
    if (!(page_directory[pdi] & PTE_P)) return 0;
    /* Recursive trick: PD[1023] maps the PD; PT for `virt` lives at
     * 0xFFC00000 + pdi*4096, its PTEs at that + pte_idx*4. */
    volatile u32* pt_entry = (volatile u32*)(0xFFC00000u + pdi * PAGE_SIZE + pte_idx * 4);
    if (!(*pt_entry & PTE_P)) return 0;
    return (*pt_entry & 0xFFFFF000u) | (virt & 0xFFFu);
}

void vmm_verify(void) {
    /* 1. The kernel alias must read back identical to the direct (identity)
     *    view of the same memory. */
    u32 off = (u32)page_directory - (u32)_kstart;
    volatile const u32* alias = (const u32*)(VMM_KALIAS_BASE + off);
    if (alias[0] != page_directory[0] ||
        alias[255] != page_directory[255] ||
        alias[1023] != page_directory[1023])
        panic("vmm: kernel alias readback mismatch", (u32)alias, 0);

    /* 2. Interrupts must still flow with PG on: wait for PIT ticks. */
    extern volatile u32 pit_ticks;
    u32 t0 = pit_ticks;
    u32 spins = 0;
    while (pit_ticks < t0 + 3) {
        __asm__ volatile("hlt");
        if (++spins > 100000000u)
            panic("vmm: PIT dead after enabling paging", 0, 0);
    }
}

u32 vmm_identity_frames(void) { return identity_frames; }
