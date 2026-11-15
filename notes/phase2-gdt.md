# Phase 2 — GDT (step 1) — in-session, Oct 2026

## Goal
Install a real 32-bit protected-mode GDT under kernel control (not the
bootloader's), so segment layout is ours and we have the foundation for a TSS
and paging. Verified live in QEMU.

## Changes
- `kernel/gdt.h`  : 8-byte `gdt_entry` + `gdt_ptr`, `gdt_init()` declaration.
- `kernel/gdt.c`  : three descriptors (null / code 0x9A / data 0x92), flat 4GiB,
  base 0, gran 0xCF (4KiB pages, 32-bit). `gdt_reload()` does `lgdt` + a far
  `ljmp $0x08` to reload CS, then reloads ds/es/fs/gs/ss to 0x10.
- `kernel/main.c` : `gdt_init()` called at the top of `kmain`, before output.
  New banner: `phase 2: GDT installed (32-bit PM, flat memory, code+data)`.
- `kernel/linker.ld`: added `PHDRS` so `ld` emits three PT_LOAD segments with
  minimal permissions — kills the previous "LOAD segment with RWX permissions"
  warning. Layout:
      LOAD  R E  -> .text    (incl. multiboot header)
      LOAD  R    -> .rodata
      LOAD  RW   -> .bss     (gdt_entries + 16 KiB stack, MemSiz 0x4028)
- `tools/build_direct.sh` + `Makefile`: compile/link `kernel/gdt.o`, `-Ikernel`.

## Build (rebuild, direct)
    i686-elf-as  --32 -Iboot -o boot/boot.o boot/boot.S
    i686-elf-gcc -ffreestanding -O2 -nostdlib -Ikernel -Ikernel/gen -c kernel/gdt.c   -o kernel/gdt.o
    i686-elf-gcc -ffreestanding -O2 -nostdlib -Ikernel -Ikernel/gen -c kernel/main.c  -o kernel/main.o
    i686-elf-ld  -T kernel/linker.ld boot/boot.o kernel/gdt.o kernel/main.o -o kernel/lindows.bin

No warnings. `kernel/lindows.bin` = 5636 bytes (was 5420; +216 for the GDT code).

## ELF verification (readelf)
- Type EXEC, Machine i386, entry 0x10000c.
- 3 PT_LOAD: R E | R | RW (no RWX). .bss is RW NOBITS, MemSiz 0x4028.
- Multiboot v1 header still intact @0x1000: magic=0x1bADB002 flags=0x3
  checksum=0xe4524ffb (sum mod 2^32 == 0).
- `nm`: gdt_init@0x100020, gdt_entries@0x100258 (BSS), stack_bottom@0x100280,
  stack_top@0x104280 — no overlap (GDT table = 24 B sits below the stack).

## Live boot (QEMU 11.1.0)
    qemu-system-i386 -kernel kernel/lindows.bin -serial file:<out> -nographic -display none

Serial (verbatim):
    LINDOWS
    profile: minimal
    boot ok: COM1 serial up, VGA text up
    phase 2: GDT installed (32-bit PM, flat memory, code+data)

The fact that execution continued past `gdt_init()` (far jump + segment reload)
and printed the Phase 2 banner is itself proof the GDT reload didn't fault. The
kernel then halts in the HLT loop as before.

## Next
Phase 2 step 2: IDT + 8259A PIC remap + a minimal timer IRQ handler, then paging.
