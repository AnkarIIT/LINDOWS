# LINDOWS — Build Plan

Phased. Each phase ends in a committed, bootable or tested artifact.

## Phase 0 — Toolchain & scaffold  [in progress]

Goal: reproducible cross-compile + emulate loop on this Windows host.

- [x] git repo initialized (master)
- [ ] i686-elf-gcc cross-compiler (prebuilt) on PATH
- [ ] nasm (assembler)
- [ ] qemu-system-i386 / x86_64 (emulator)
- [ ] `make`
- [ ] "Hello, protected mode" prints via VGA in QEMU

Notes
  - On Windows, build a *cross*-compiler (i686-elf / x86_64-elf), NOT the host
    gcc. Host gcc emits Windows PE/COFF; a kernel must be ELF.
  - OSDev convention: start 32-bit (i686-elf + GRUB multiboot), add 64-bit later.
  - Prebuilt option for Windows: lordmilko/i686-elf-tools on GitHub.

## Phase 1 — Bootstrapping a 32-bit kernel

Goal: first kernel boot under QEMU, VGA text output.

- stage0: bootloader hands off via Multiboot (GRUB) or a minimal 16->32 bit stub
- kernel entry in C (freestanding), -ffreestanding, -nostdlib
- linker script places kernel at 1MiB, higher-half later
- printk -> VGA text buffer
- Artifact commit: a bootable ISO that prints a banner

## Phase 2 — Core kernel primitives

Goal: a basis for everything that follows.

- GDT (protected mode segments)
- IDT + PIC/APIC interrupt handling
- Physical + virtual paging, higher-half kernel
- Basic heap (linked-list, then better algorithms per profile)
- Spinlocks / pre-emptive round-robin scheduler

## Phase 3 — 64-bit long mode

Goal: native 64-bit kernel.

- x86_64-elf toolchain switch
- Long mode entry, PML4 paging
- 64-bit calling convention, syscalls
- Limine or BOOTBOOT bootloader (GRUB can't take a 64-bit kernel directly)

## Phase 4 — Device model

Goal: hardware abstraction.

- PCI enumeration
- AHCI (disk), USB (XHCI), PS/2 / keyboard
- Framebuffer instead of VGA text
- Basic networking driver (e1000/virtio-net)

## Phase 5 — Userspace & profile runtime

Goal: the "user-profile-optimized" core of LINDOWS.

- ELF loader + exec
- A small C library (freestanding)
- Service manager: each profile = a manifest of services to start
- Kernel tunables exposed as sysctls; profile daemon sets them on boot
  (scheduler, vm, io) and adapts from usage stats
- Per-profile news/tool feed integration at the userspace shell
- Artifact: `developer` profile boots with compiler toolchain available and
  dev-news feed populated; `gamer` profile tunes scheduler latency + only starts
  graphics/input/audio.

## Phase 6 — Polishing & ports

Goal: stability + a second architecture.

- SMP bringup
- Rust support for kernel crates
- Port to RISC-V (profile system designed for multi-arch)
- Filesystem driver (ext2 first, then own design)

## Risk notes

  - Building a full OS is a multi-month project; phases here are milestones, not
    days. Each phase is independently bootable/tested so progress is real.
  - Toolchain is the only hard blocker this session; once QEMU+NASM+cross-gcc are
    in place, Phase 1 artifact is days of work.
