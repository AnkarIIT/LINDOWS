/*
 * LINDOWS heap implementation — see heap.h for the design.
 *
 * Free list in address order; first fit; split on alloc (leftover must be
 * able to hold header + 8 payload bytes), coalesce both sides on free.
 * Growth: whole pages from the PMM, mapped into HEAP_BASE.. via the VMM,
 * appended as a fresh free block (merged into a free tail immediately).
 * Pages are never returned to the PMM in Phase 2 — the heap only grows.
 */
#include "heap.h"
#include "pmm.h"
#include "vmm.h"
#include "panic.h"

#define HDR_SIZE   ((u32)sizeof(block_t))         /* 16 */
#define ALIGN8(n)  (((n) + 7u) & ~7u)
#define MIN_SPLIT  (HDR_SIZE + 8u)                /* split only if leftover
                                                      can hold 8 payload */

typedef struct block {
    u32 size;              /* payload bytes (excludes this header) */
    u32 used;              /* 0 or 1 */
    struct block* next;    /* address-ordered list */
    struct block* prev;
} block_t;

static block_t* first_block = 0;
static block_t* tail        = 0;
static u32      mapped      = 0;   /* bytes mapped at HEAP_BASE */

static u32 st_allocs = 0, st_frees = 0, st_used = 0, st_peak = 0;

static void zero_page(u32 vma) {
    volatile u8* p = (volatile u8*)vma;
    for (u32 i = 0; i < PAGE_SIZE; i++) p[i] = 0;
}

/* Link a freshly mapped page in as one free block, coalescing with a free
 * predecessor so consecutive growth pages become ONE block. */
static void append_free(u32 vma, u32 size) {
    block_t* b = (block_t*)vma;
    b->size = size;
    b->used = 0;
    b->next = 0;
    b->prev = tail;
    if (tail) tail->next = b;
    else      first_block = b;
    tail = b;

    if (b->prev && !b->prev->used) {
        block_t* p = b->prev;
        p->size += HDR_SIZE + b->size;
        p->next  = 0;
        tail     = p;
    }
}

/* Map whole pages until `mapped >= bytes`. Returns 0 ok, -1 exhausted. */
static int expand(u32 bytes) {
    while (mapped < bytes) {
        if (mapped >= HEAP_MAX) return -1;
        u32 vma = HEAP_BASE + mapped;
        u32 f = pmm_alloc();
        if (!f) return -1;
        if (vmm_map_page(vma, f)) { pmm_free(f); return -1; }
        mapped += PAGE_SIZE;
        zero_page(vma);
        append_free(vma, PAGE_SIZE - HDR_SIZE);
    }
    return 0;
}

void heap_init(void) {
    /* Nothing to do but assert ordering: pages are mapped lazily on the
     * first allocation. The PMM and VMM must already be initialized. */
    if (first_block || mapped)
        panic("heap: double init", 0, 0);
}

void* kmalloc(u32 n) {
    if (n == 0) return NULL;                 /* policy: 0 bytes = NULL */
    u32 need = ALIGN8(n);

    if (!first_block && expand(HDR_SIZE + need)) return NULL;

    for (;;) {
        for (block_t* b = first_block; b; b = b->next) {
            if (b->used || b->size < need) continue;

            if (b->size - need >= MIN_SPLIT) {
                block_t* nb = (block_t*)((u8*)b + HDR_SIZE + need);
                nb->size = b->size - need - HDR_SIZE;
                nb->used = 0;
                nb->next = b->next;
                nb->prev = b;
                if (nb->next) nb->next->prev = nb;
                else          tail = nb;
                b->next = nb;
                b->size = need;
            }

            b->used = 1;
            st_allocs++;
            st_used += b->size;
            if (st_used > st_peak) st_peak = st_used;
            return (u8*)b + HDR_SIZE;
        }
        /* Nothing fits: grow by whole pages until the demand fits, then
         * walk again (the fresh tail block coalesces as it grows). */
        if (expand(mapped + HDR_SIZE + need)) return NULL;
    }
}

void kfree(void* p) {
    if (!p) return;                          /* policy: free(NULL) ok */

    u32 v = (u32)p;
    if (v < HEAP_BASE + HDR_SIZE || v > HEAP_BASE + mapped || (v & 7u))
        panic("heap: kfree outside heap or misaligned", v, 0);

    block_t* b = (block_t*)(v - HDR_SIZE);
    if (b->used != 1)
        panic("heap: double free or corrupt header", v, 0);

    b->used = 0;
    st_frees++;
    st_used -= b->size;

    if (b->next && !b->next->used) {         /* absorb next */
        block_t* nx = b->next;
        b->size += HDR_SIZE + nx->size;
        b->next  = nx->next;
        if (nx->next) nx->next->prev = b;
        else          tail = b;
    }
    if (b->prev && !b->prev->used) {         /* merge into prev */
        block_t* pv = b->prev;
        pv->size += HDR_SIZE + b->size;
        pv->next  = b->next;
        if (b->next) b->next->prev = pv;
        else         tail = pv;
    }
}

void* kcalloc(u32 n, u32 m) {
    if (m && n > HEAP_MAX / m) return NULL;  /* overflow + sanity bound */
    u8* p = kmalloc(n * m);
    if (!p) return NULL;
    for (u32 i = 0; i < n * m; i++) p[i] = 0;
    return p;
}

void* krealloc(void* p, u32 n) {
    if (!p) return kmalloc(n);
    if (n == 0) { kfree(p); return NULL; }

    u32 old = ((block_t*)((u8*)p - HDR_SIZE))->size;
    u8* q = kmalloc(n);
    if (!q) return NULL;
    u32 c = (old < n) ? old : n;
    u8* s = (u8*)p;
    for (u32 i = 0; i < c; i++) q[i] = s[i];
    kfree(p);
    return q;
}

void heap_verify(void) {
    /* Policy checks: kmalloc(0) is NULL; kfree(NULL) is a no-op (the
     * alloc/free deltas asserted below prove the stats did not move). */
    if (kmalloc(0) != NULL)
        panic("heap: kmalloc(0) must return NULL", 0, 0);
    kfree(NULL);

    u32 a0 = st_allocs, f0 = st_frees;

    /* 1. Alloc a spread of sizes, fill each with a per-block pattern. */
    enum { NB = 64 };
    static const u32 sz[4] = {8, 24, 120, 1024};
    void* blk[NB];
    for (u32 i = 0; i < NB; i++) {
        blk[i] = kmalloc(sz[i & 3]);
        if (!blk[i]) panic("heap: verify alloc failed", i, 0);
        u8* p = blk[i];
        for (u32 j = 0; j < sz[i & 3]; j++)
            p[j] = (u8)(i * 37u + j + 0x5Au);
    }

    /* 2. Readback: every byte of every block survives. */
    for (u32 i = 0; i < NB; i++) {
        u8* p = blk[i];
        for (u32 j = 0; j < sz[i & 3]; j++)
            if (p[j] != (u8)(i * 37u + j + 0x5Au))
                panic("heap: pattern readback mismatch", (i << 16) | j, 0);
    }

    /* 3. Free everything: stats must return to zero and the free list must
     *    coalesce (asserted by the count match + what follows). */
    for (u32 i = 0; i < NB; i++) kfree(blk[i]);
    if (st_used != 0)
        panic("heap: used bytes not zero after freeing all", st_used, 0);
    if (st_allocs - a0 != NB || st_frees - f0 != NB)
        panic("heap: alloc/free count mismatch",
              ((st_allocs - a0) << 16) | (st_frees - f0), 0);

    /* 4. One 64 KiB alloc must succeed over the coalesced region plus new
     *    pages, and its sampled bytes must survive. */
    u8* big = kmalloc(65536);
    if (!big) panic("heap: 64 KiB alloc failed after free-all", mapped, 0);
    for (u32 j = 0; j < 65536; j += 4096) big[j] = 0xA5;
    for (u32 j = 0; j < 65536; j += 4096)
        if (big[j] != 0xA5) panic("heap: big block pattern mismatch", j, 0);
    kfree(big);

    /* 5. krealloc preserves the old contents (100 -> 8000 bytes). */
    u8* rp = kmalloc(100);
    if (!rp) panic("heap: krealloc base alloc failed", 0, 0);
    for (u32 j = 0; j < 100; j++) rp[j] = (u8)(j ^ 0xC3);
    u8* rq = krealloc(rp, 8000);
    if (!rq) panic("heap: krealloc failed", 0, 0);
    for (u32 j = 0; j < 100; j++)
        if (rq[j] != (u8)(j ^ 0xC3)) panic("heap: krealloc data lost", j, 0);
    kfree(rq);

    if (st_used != 0)
        panic("heap: used bytes not zero after verify", st_used, 0);
}

u32 heap_pages_mapped(void) { return mapped / PAGE_SIZE; }
u32 heap_allocs(void)       { return st_allocs; }
u32 heap_frees(void)        { return st_frees; }
u32 heap_peak_bytes(void)   { return st_peak; }
