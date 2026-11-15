# LINDOWS

A from-scratch, user-profile-optimized operating system.

LINDOWS is built bottom-up from zero (no Linux/UNIX code), currently a 32-bit
Multiboot kernel with a planned move to x86_64 long mode (PLAN.md Phase 3).
Its defining idea: the OS is compiled, booted, and tuned per *user profile* /
usecase. At first boot you pick a usecase — `developer`, `gamer`, `creator`,
`server`, `minimal`, … — and that profile drives:

  - which kernel subsystems and drivers are compiled in
  - scheduler latency vs. throughput tuning
  - memory management strategy (aggressive cache vs. low-RAM)
  - I/O scheduler choice
  - which userspace services start by default
  - per-usecase news/tool feeds exposed to the user

Profiles are not fixed: usage is fed back in to adapt the running system
(lightweight, profile-aware optimizer). Think "one kernel config, many tuned
 personalities", selected at setup time.

## Layout

  boot/        first-stage loader + entry stubs
  kernel/      kernel sources, staged by subsystem
  tools/       build scripts, cross-compiler bootstrap
  profiles/    per-usecase build/boot/runtime config
  papers/      reference material and design reading
  notes/       dev journal
  outputs/     drafts/ and plans/ — produced documents
  (tests/ does not exist yet; the boot smoke test in tools/smoke_test.sh is
   the current test suite)

## Quick start (this host — no make, no grub-mkrescue)

  tools/build_direct.sh              # canonical build -> kernel/lindows.bin
  tools/smoke_test.sh                # build + headless QEMU boot + assertions

(The Makefile mirrors the script for hosts that have make. `make iso` needs
grub-mkrescue, which is not installed yet — the ISO artifact is deferred.)

See PLAN.md for the phased roadmap.
