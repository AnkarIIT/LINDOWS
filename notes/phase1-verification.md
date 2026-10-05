# Phase 1 verification — 2026-10-05

Achieved: a bootable Multiboot ELF LINDOWS kernel, built from scratch
with a prebuilt i686-elf cross-toolchain (GCC 15.2.0 / Binutils 2.45),
and **booted live in QEMU 11.1.0** with serial + VGA verification.

## Build (direct — GNU make is NOT installed on this host)

    i686-elf-as  --32 -Iboot -o boot/boot.o boot/boot.S
    i686-elf-gcc -ffreestanding -O2 -Wall -Wextra -nostdlib -Ikernel/gen -c kernel/main.c -o kernel/main.o
    i686-elf-ld  -T kernel/linker.ld boot/boot.o kernel/main.o -o kernel/lindows.bin

The toolchain lives at tools/i686-elf/ (gitignored; ~2.8 GB — keep it local only).

## Bugs found & fixed during build

  - boot.S / multiboot.inc were written in NASM syntax (%include, BITS 32,
    equ, dd) but the shipped toolchain is GNU as. Rewrote to gas syntax
    (.include, .code32, .set, .long).
  - `as -f elf32` is a NASM idiom; GNU as uses `--32` (-f meant "no
    preprocess", which made as eat "elf32" as a source file and cascade).
  - `i686-elf-ld` is invoked directly; -nostdlib/-nodefaultlibs/-nostartfiles
    are GCC-driver flags and are rejected by ld. Dropped them — a bare ld link
    already pulls in no stdlib. (Makefile fixed accordingly.)
  - **No stack was installed.** Multiboot v1 leaves registers undefined, so
    trusting the loader's %esp is non-portable. linker.ld now reserves a
    16 KiB stack in .bss (stack_bottom/stack_top) and _start sets
    `mov $stack_top, %esp; xor %ebp, %ebp` before calling kmain.
  - **VGA-only output made headless verification impossible.** main.c now
    mirrors every character to COM1 (115200 8N1), so `qemu -serial` captures
    the boot text.

## Verification of kernel/lindows.bin (5420 bytes)

  - readelf -h : ELF32, EXEC, Intel 80386, entry 0x10000c (_start).
  - Multiboot v1 header @ VMA 0x100000 (file offset 0x1000):
        magic      = 0x1bADB002
        flags      = 0x00000003
        checksum   = 0xe4524ffb   -> (magic+flags+checksum) mod 2^32 == 0  OK
  - Symbols: multiboot_header@0x100000, _start@0x10000c,
             stack_bottom@0x100150, stack_top@0x104150 (16 KiB).
  - .bss grew 0x4 -> 0x4010 (the reserved stack). PT_LOAD memsz covers it,
    so GRUB/QEMU zero-fill it.
  - A second 0x1BADB002 occurrence at file offset 0x1178 is the absolute
    symbol `MULTIBOOT_HEADER_MAGIC` inside .symtab. It lies OUTSIDE the only
    PT_LOAD segment (file 0x1000..0x113f), so it is never loaded into memory
    and GRUB never sees it. Harmless.

## Live boot (QEMU 11.1.0) — VERIFIED

    qemu-system-i386 -kernel kernel/lindows.bin -nographic

Serial output captured verbatim:

    LINDOWS
    profile: minimal
    boot ok: COM1 serial up, VGA text up
    phase 1 complete. halting.

### Profile mechanism proven end-to-end

Booted twice, once per profile, and diffed both the serial log and the VGA
framebuffer (monitor `screendump`):

  - serial diff  : exactly one line differs
        - profile: minimal
        + profile: developer
  - pixel diff   : bounding box (81, 18, 170, 31) = columns 9..18, row 1 only.
    349 of 288000 pixels changed (0.121%). "profile: " occupies columns 0..8,
    so the profile name begins exactly at column 9 (x=81) — the diff starts
    there and nowhere else.

So the build-time profile selection really does reach the screen, and nothing
else changes between profiles.

### Emulator install (no admin required)

`choco install qemu` fails here ("System error 5", no admin). Instead:
downloaded Stefan Weil's prebuilt Windows build (qemu-w64-setup-20260811.exe,
197 MB, SHA-512 verified against the publisher's .sha512) and ran it with

    qemu-setup.exe /S /D=C:\OS\tools\qemu

which installs to a user-writable directory. No admin, no Program Files, no
system changes. Uninstall = delete the folder. (1.2 GB on disk.)

## Known limitation

  - The build bakes the profile *name* into the kernel via
    `printf '#define PROFILE_NAME "%s"' "$PROFILE`. The YAML files in
    profiles/ are NOT parsed by the build — they are documentation of the
    intended profile model, not yet a build input. Wiring the real profile
    engine (from C:\Users\bamba\os-project) into the kernel build is Phase 2
    work, so "subsystem flags" remain a future step.
