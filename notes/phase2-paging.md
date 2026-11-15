# Phase 2 — PMM + paging — in-session, Nov 2026

## Goal
Give the kernel physical + virtual memory management, with paging actually
enforced and verified — not just "enabled".

## What was built

- `kernel/pmm.c/.h` — bitmap frame allocator (4 KiB frames, capped at 1 GiB
  of tracked RAM to bound BSS at 32 KiB of bitmap). Seeded from the multiboot
  mmap (flags bit 6), with mem_upper fallback (flags bit 0). Frames < 1 MiB
  permanently used (BIOS/EBDA/VGA); kernel image region [_kstart,_kend)
  reserved (covers the bitmap itself). API: pmm_alloc/free/installed/free/
  used_mmap. No locking yet — arrives with the scheduler.
- `kernel/vmm.c/.h` — 32-bit paging:
    * identity map of ALL tracked RAM (supervisor RW) — PTs allocated from
      the PMM, written pre-PG (linear==physical then),
    * kernel alias: 0xC0000000..0xC0100000 -> phys [_kstart..+1 MiB) —
      the runway for the higher-half relocation,
    * recursive PD: entry 1023 points at the PD (PTs visible at 0xFFC00000),
    * CR3 loaded, CR0.PG | CR0.WP set.
    * vmm_verify(): alias readback must equal the direct view + PIT must
      still tick under PG.
- `kernel/panic.c` — now prints CR2 (the #PF faulting address).
- `kernel/main.c` — pmm_init(mb_info) -> vmm_init() -> vmm_verify(); boot
  log reports frames installed/free and paging state. New fault-injection
  hook: -DLINDOWS_PFAULT_TEST derefs 0x40000000 (1 GiB, unmapped).

## Bugs the tests caught (this is why we test)

1. mmap parse was off by one dword (entries begin with their size field;
   I read the struct at p+4) -> 0 frames installed. Raw-dword debug dump of
   the QEMU map exposed the true layout; fixed both passes.
2. `page_directory[base >> 22]` with base counted in FRAMES — every PT was
   written to PD slot 0, evicting the mapping the CPU was executing from =>
   silent triple-fault REBOOT LOOP (log showed the banner ~30 times, no
   panic: the fault handler itself was unmapped). Fixed to base/1024.
3. Stale .o in manual fault-test builds (panic.o lacked the CR2 line until
   rebuilt) — reminder that ad-hoc incremental builds need care; the smoke
   test always builds everything.

## Smoke test hardening
`tools/smoke_test.sh` now FAILS if the banner appears more than once
(reboot loop / triple fault detector) — this would have caught bug #2
automatically instead of by eyeballing the log.

## Verification (QEMU 9.2.0)
- Normal boot: SMOKE TEST PASSED —
    pmm: 32736 frames installed, 32462 free (mmap path)
    vmm: paging ON (CR0.PG|WP): identity 127 MiB RAM, kernel alias
         @0xC0000000, recursive PD @0xFFFFF000
- -DLINDOWS_PFAULT_TEST: clean PANIC —
    page fault (#PF), vector 0xE, error 0x0 (not-present, supervisor read),
    EIP at the faulting load, CR2 = 0x40000000, GPR dump, system halted.

## Next
Higher-half link relocation (link at 0xC0100000, boot stub builds early
tables, then switch stacks/EIP), then heap + scheduler to close Phase 2.
