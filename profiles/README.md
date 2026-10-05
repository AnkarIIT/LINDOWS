# LINDOWS profile system

A *profile* (aka usecase) is the single input that makes LINDOWS user-profile-
optimized. Everything below is resolved from one profile name chosen at setup.

## On disk: profiles/<name>.profile

A small manifest (YAML/JSON — kept dependency-free, hand-parsed in C for boot):

```yaml
# profiles/developer.profile
name: developer
arch: x86_64
kernel:
  subsystems:
    - mm_heap_ptmalloc       # enable a real allocator
    - sched_latency        # low-latency scheduler
    - net_tcp_udp
    - fs_ext2
  tunables:
    vm.swappiness: 10                 # keep cache, devs have RAM
    vm.dirty_ratio: 20
    sched.latency: 2                  # ms, interactive devs editing
  build:
    with_toolchain: true              # ship a toolchain into the build
    debug: true
runtime:
  services:                           # what the service manager starts
    - sh
    - devd                           # toolchain/dev news daemon
    - netd
  feeds:                             # per-profile content feeds
    - type: arxiv
      category: cs.OS
      cadence: daily
    - type: rss
      url: https://lwn.net/rss/feed/ # kernel/dev news
      cadence: hourly
```

## How it flows

1. **Setup time** — picker writes `profiles/active` (a symlink/flag) to the boot
   partition and bakes the profile into the kernel ELF (build-time) + a runtime
   manifest the kernel passes to `init`.
2. **Build time** — Makefile:
     `make KERNEL_PROFILE=developer`
   reads `profiles/developer.profile`, emits `kernel/profile_gen.h` with the
   `#define PROFILE_<SUBSYS>` flags, sets linker/debug flags, and (for the
   `with_toolchain` feature) stages a cross-compiler into the ISO.
3. **Boot time** — kernel reads the manifest, applies tunables (early sysctl
   table set before the scheduler starts), starts only the listed services.
4. **Runtime** — `lprofd` (profile daemon) keeps the selected services + feeds
   alive, and logs usage; a weekly pass nudges tunables if behavior drifts from
   the profile's intent (the "profile-aware optimizer").

## Profiles in this repo

  developer   — compilers/toolchain, dev-news feeds, low-latency interactive
  gamer       — latency-tuned scheduler, graphics/audio/input only
  creator     — framebuffer + audio + media codecs, throughput cache
  server      — throughput scheduler, network/fs tuned, no desktop services
  minimal     — fewest subsystems, smallest footprint, single service

Every profile is valid for every phase — the build simply greps out the
subsystems not enabled, so a `minimal` build of Phase 1 is smaller than a
`developer` one. That's the whole point: the OS tailors itself to the usecase.
