# LINDOWS developer profile

name: developer
arch: x86_64
kernel:
  subsystems:
    - mm_heap
    - sched_latency
    - net_tcp_udp
    - fs_ext2
  tunables:
    vm.swappiness: 10
    vm.dirty_ratio: 20
    sched.latency_ms: 2
  build:
    with_toolchain: true
    debug: true
runtime:
  services:
    - sh
    - devd
    - netd
  feeds:
    - type: rss
      url: https://lwn.net/rss/feed/
      cadence: hourly
