# Phase 2 — Kernel heap (kmalloc over PMM+VMM)

Goal: dynamic memory for the kernel. First-fit free list with split and
coalesce, grown on demand from pages taken via the PMM and mapped via the
VMM. API: `kmalloc / kfree / kcalloc / krealloc` (kernel/heap.h, heap.c).

## Placement decision

Heap home = **0xC8000000 (PD index 800), capped at 4 MiB** (one PD entry:
0xC8000000..0xC8400000). Rationale: putting it inside the kernel window
(0xC0000000..0xC0400000, which is the *identity* map aliased) would have the
heap's remappings quietly destroy identity aliases of those linear pages —
breaking the "identity map covers all tracked RAM" invariant that vmm_verify
checks. Its own region means its PT is freshly allocated by vmm_map_page and
nothing pre-existing is touched.

## Design notes

- Block header is 16 bytes: `{u32 size; u32 used; struct block *next, *prev;}`
  — size counts payload only; payload pointer = block + 16; blocks are kept
  in a single address-ordered list (also serves as the free list).
- First fit + split if leftover >= header + 8 payload bytes; kfree coalesces
  with both neighbors, so adjacent frees always merge.
- Growth: `expand()` maps whole pages (pmm_alloc + vmm_map_page), zeroes
  them, and appends one free block per page — immediately merged into a free
  tail, so repeated growth yields ONE big free block.
- Policies: kmalloc(0) == NULL; kfree(NULL) no-op; kfree validates that the
  pointer is inside the mapped heap, 8-aligned, and marks a used block —
  anything else panics (double free / corruption cannot pass silently).
- kcalloc bounds n*m by HEAP_MAX before multiplying; krealloc allocates,
  copies min(old,new), frees.
- No page is ever returned to the PMM in Phase 2 (heap only grows); a
  shrink/trim pass can come later if profiles need it.

## Bug found during bring-up

- First build failed compile: heap.c used a 3-arg `panic(reason, val, val)`
  shape — the real signature is `panic(reason, vector, regs*)` where regs is
  a pointer (0 when no interrupt frame). Where two numbers matter, they are
  packed into the one u32 (`(i << 16) | j`). Also `NULL` did not exist in
  the freestanding tree — added to kernel/types.h (`((void*)0)`), since
  vmm.c had been silently using bare `0` for pointers.

## Verification (all live, QEMU 9.2.0)

- `heap_verify()` runs on every boot and panics on the first failure:
  1. kmalloc(0) == NULL; kfree(NULL) no-op (alloc/free deltas asserted after)
  2. 64 blocks sized {8,24,120,1024} cycling — filled with per-block
     patterns, every byte read back
  3. free-all → used bytes return to exactly 0, alloc/free counts match
  4. one 64 KiB alloc over the coalesced region + new pages, sampled
     readback
  5. krealloc(100 → 8000) preserves all 100 old bytes
- Boot banner: `heap verified: 67 allocs, peak 65536 bytes, 22 pages
  @0xC8000000` (67 = 64 pattern + big + 2 krealloc path).
- `tools/smoke_test.sh` now FAILs if the "heap verified" line is missing.
- Regression: `-DLINDOWS_PFAULT_TEST` still produces a clean PANIC with
  `cr2 = 0x40000000` (heap does not disturb the fault path).
