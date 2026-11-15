/*
 * LINDOWS PMM implementation — bitmap frame allocator over the multiboot
 * memory map. See pmm.h for the design.
 */
#include "pmm.h"
#include "panic.h"
#include "console.h"

/* Multiboot info struct (fixed ABI). Offsets: flags 0, mem_lower 4,
 * mem_upper 8, ..., mmap_length 44, mmap_addr 48. */
typedef struct {
    u32 flags;
    u32 mem_lower;
    u32 mem_upper;
    u32 boot_device;
    u32 cmdline;
    u32 mods_count;
    u32 mods_addr;
    u32 syms[4];
    u32 mmap_length;
    u32 mmap_addr;
} __attribute__((packed)) mb_info_t;

#define MB_FLAG_MEM      0x0001u
#define MB_FLAG_MMAP     0x0040u

/* One memory-map entry: <u32 size> then the body. */
typedef struct {
    u32 size;      /* size of THIS body (typically 20) */
    u64 base;
    u64 len;
    u32 type;      /* 1 = usable RAM */
} __attribute__((packed)) mb_mmap_t;

#define MB_MMAP_USABLE 1u

/* Cap: track at most 1 GiB of RAM -> 256 Ki frames -> 32 KiB bitmap in BSS. */
#define PMM_MAX_FRAMES 262144u
#define LOW_MEM_FRAMES 256u          /* 0..1 MiB: BIOS/EBDA/VGA - never free  */

static u8 frame_bitmap[PMM_MAX_FRAMES / 8];   /* BSS: zero-filled by loader */
static u32 installed_frames = 0;
static u32 free_count       = 0;
static u32 used_mmap        = 0;

static inline void mark_used(u32 frame) {
    frame_bitmap[frame >> 3] |= (u8)(1u << (frame & 7));
}

static inline void mark_free(u32 frame) {
    frame_bitmap[frame >> 3] &= (u8)~(1u << (frame & 7));
}

static inline u32 is_used(u32 frame) {
    return (frame_bitmap[frame >> 3] >> (frame & 7)) & 1u;
}

static void mark_range_used(u32 frame_from, u32 frame_to) {
    for (u32 f = frame_from; f < frame_to && f < PMM_MAX_FRAMES; f++)
        mark_used(f);
}

static void mark_range_free(u32 frame_from, u32 frame_to) {
    for (u32 f = frame_from; f < frame_to && f < PMM_MAX_FRAMES; f++)
        if (is_used(f)) { mark_free(f); free_count++; }
}

void pmm_init(u32 mb_info) {
    const mb_info_t* mb = (const mb_info_t*)mb_info;
    u64 highest = 0;

#ifdef LINDOWS_DEBUG
    console_puts("debug: mb_info@", CON_COLOR_RED);
    console_print_hex(mb_info, CON_COLOR_WHITE);
    console_puts(" flags=", CON_COLOR_RED);
    console_print_hex(mb ? mb->flags : 0, CON_COLOR_WHITE);
    if (mb && (mb->flags & MB_FLAG_MEM)) {
        console_puts(" mem_upper=", CON_COLOR_RED);
        console_print_u32(mb->mem_upper, CON_COLOR_WHITE);
    }
    if (mb && (mb->flags & MB_FLAG_MMAP)) {
        console_puts(" mmap_addr=", CON_COLOR_RED);
        console_print_hex(mb->mmap_addr, CON_COLOR_WHITE);
        console_puts(" mmap_len=", CON_COLOR_RED);
        console_print_u32(mb->mmap_length, CON_COLOR_WHITE);
        console_puts("\r\n", CON_COLOR_WHITE);
        const u8* dp   = (const u8*)mb->mmap_addr;
        const u8* dend = dp + mb->mmap_length;
        while (dp + 4 <= dend) {
            const mb_mmap_t* e = (const mb_mmap_t*)dp;
            console_puts("  mmap: type=", CON_COLOR_RED);
            console_print_u32(e->type, CON_COLOR_WHITE);
            console_puts(" base=", CON_COLOR_RED);
            console_print_hex((u32)e->base, CON_COLOR_WHITE);
            console_puts(" len=", CON_COLOR_RED);
            console_print_hex((u32)e->len, CON_COLOR_WHITE);
            console_puts("\r\n", CON_COLOR_WHITE);
            dp += 4 + e->size;
        }
    }
    console_puts("\r\n", CON_COLOR_WHITE);
#endif

    /* 1. Discover installed RAM. Entries begin with their size field, so the
     *    struct address IS the entry address (size@0, base@4, len@12, type@20). */
    if (mb && (mb->flags & MB_FLAG_MMAP) && mb->mmap_addr && mb->mmap_length) {
        used_mmap = 1;
        const u8* p   = (const u8*)mb->mmap_addr;
        const u8* end = p + mb->mmap_length;
        while (p + 4 <= end) {
            const mb_mmap_t* e = (const mb_mmap_t*)p;
            if (e->type == MB_MMAP_USABLE && e->base + e->len > highest)
                highest = e->base + e->len;
            p += 4 + e->size;
        }
    } else if (mb && (mb->flags & MB_FLAG_MEM)) {
        /* Fallback: mem_upper = KiB of RAM above 1 MiB. */
        highest = 0x100000ull + ((u64)mb->mem_upper * 1024ull);
    } else {
        panic("pmm: multiboot info has no memory info", 0, 0);
    }

    u64 frames = (highest + PAGE_SIZE - 1) / PAGE_SIZE;
    installed_frames = (frames > PMM_MAX_FRAMES) ? PMM_MAX_FRAMES : (u32)frames;

    /* 2. Everything starts used; then free the usable >= 1 MiB ranges. */
    free_count = 0;
    mark_range_used(0, installed_frames);

    if (used_mmap) {
        const u8* p   = (const u8*)mb->mmap_addr;
        const u8* end = p + mb->mmap_length;
        while (p + 4 <= end) {
            const mb_mmap_t* e = (const mb_mmap_t*)p;
            if (e->type == MB_MMAP_USABLE) {
                u32 from = (u32)((e->base + PAGE_SIZE - 1) / PAGE_SIZE);
                u32 to   = (u32)((e->base + e->len) / PAGE_SIZE);
                if (from < LOW_MEM_FRAMES) from = LOW_MEM_FRAMES;
                mark_range_free(from, to);
            }
            p += 4 + e->size;
        }
    } else {
        mark_range_free(LOW_MEM_FRAMES, installed_frames);
    }

    /* 3. Reserve the kernel image at its PHYSICAL location. _kphys_* are
     *    LMA symbols from linker.ld (the _kstart/_kend symbols are
     *    higher-half VMAs and must NOT be used for physical bookkeeping). */
    extern const u8 _kphys_start[];
    extern const u8 _kphys_end[];
    u32 kf = (u32)_kphys_start >> 12;
    u32 ke = (((u32)_kphys_end) + PAGE_SIZE - 1) >> 12;
    for (u32 f = kf; f < ke; f++)
        if (!is_used(f)) { mark_used(f); free_count--; }
}

u32 pmm_alloc(void) {
    for (u32 f = LOW_MEM_FRAMES; f < installed_frames; f++) {
        if (!is_used(f)) {
            mark_used(f);
            free_count--;
            return f << 12;
        }
    }
    return 0;
}

void pmm_free(u32 phys) {
    u32 f = phys >> 12;
    if (f < LOW_MEM_FRAMES || f >= installed_frames)
        panic("pmm_free: frame out of range", phys, 0);
    if (!is_used(f))
        panic("pmm_free: double free", phys, 0);
    mark_free(f);
    free_count++;
}

u32 pmm_installed_frames(void) { return installed_frames; }
u32 pmm_free_frames(void)      { return free_count; }
u32 pmm_used_mmap(void)        { return used_mmap; }
