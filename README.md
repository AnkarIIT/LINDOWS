# LINDOWS

A from-scratch, user-profile-optimized operating system.

LINDOWS is built bottom-up from zero (no Linux/UNIX code) on x86_64. Its
defining idea: the OS is compiled, booted, and tuned per *user profile* /
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
  tests/       unit + integration tests

## Quick start (after toolchain)

  make iso        # build a bootable ISO
  make run        # launch in QEMU
  make test       # run the test suite

See PLAN.md for the phased roadmap.
