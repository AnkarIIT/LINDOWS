# Phase 1 verification — 2026-10-05

Achieved: a bootable Multiboot ELF LINDOWS kernel, built from scratch with a
prebuilt i686-elf cross-toolchain (GCC 15.2.0 / Binutils 2.45). First real
artifact, verified.

## Build (direct — GNU make is NOT installed on this host)

    i686-elf-as  --32 -Iboot -o boot/boot.o boot/boot.S
    i686-elf-gcc -ffreestanding -O2 -Wall -Wextra -nostdlib -Ikernel/gen -c kernel/main.c -o kernel/main.o
    i686-elf-ld  -T kernel/linker.ld boot/boot.o kernel/main.o -o kernel/lindows.bin

The toolchain lives at tools/i686-elf/ (gitignored; ~900 MB — keep it local only).

## Bugs found & fixed during build

  - boot.S / multiboot.inc were written in NASM syntax (%include, BITS 32, equ,
    dd) but the shipped toolchain is GNU as. Rewrote to gas syntax
    (.include, .code32, .set, .long).
  - `as -f elf32` is a NASM idiom; GNU as uses `--32` (-f meant "no
    preprocess", which made as eat "elf32" as a source file and cascade).
  - `i686-elf-ld` is invoked directly; `-nostdlib/-nodefaultlibs/-nostartfiles`
    are GCC-driver flags and are rejected by ld. Dropped them — a bare ld link
    already pulls in no stdlib. (Makefile fixed accordingly.)

## Verification of kernel/lindows.bin (5232 bytes)

  - readelf -h : ELF32, EXEC, Intel 80386, entry 0x10000c (_start).
  - Multiboot v1 header @ VMA 0x100000 (file offset 0x1000):
        magic      = 0x1bADB002
        flags      = 0x00000003
        checksum   = 0xe4524ffb   -> (magic+flags+checksum) mod 2^32 == 0  OK
  - Symbols: multiboot_header@0x100000, _start@0x10000c, kmain@0x1000d0.
  - .rodata holds "LINDOWS\r\n", "profile: ", "minimal" — the profile name is
    baked in at build time from profiles/<usecase>.profile.
  - Disassembly: _start = `cli; call kmain`; puts writes `char|colour` to
    0xB8000 (VGA text) via `mov %cx, 0xb8000(%eax)` (col doubled for byte
    offset, colour pre-shifted); kmain falls into `hlt; jmp hlt`. Correct.
  - Links with no libgcc and no undefined symbols.

## Blocked / open

  - Live boot demo needs QEMU. `choco install qemu` fails (no admin rights:
    "System error 5"); the portable-QEMU fetch was consent-gated and I'm not
    re-attempting it without the user's go-ahead. Once an emulator exists:
        qemu-system-i386 -kernel kernel/lindows.bin
    should print "LINDOWS profile: minimal" to the VGA screen.
