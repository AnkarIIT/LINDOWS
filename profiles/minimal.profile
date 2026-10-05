# LINDOWS minimal profile (phase 0/1 default)

name: minimal
arch: i686
kernel:
  subsystems: []
  tunables: {}
  build:
    with_toolchain: false
    debug: false
runtime:
  services:
    - sh
  feeds: []
