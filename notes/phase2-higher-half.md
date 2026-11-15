# Phase 2 — Higher-half link relocation

Goal: link the kernel at a VMA above 3 GiB while the multiboot loader still
loads it at physical ~1 MiB, so `phys = vma - 0xC0000000` inside the kernel
image. This is the standard higher-half setup: the kernel keeps its own
address space identity, low memory can be unmapped later, and user space
gets the bottom of every address space.

## Final layout (verified in QEMU)

```
.phys 0x100000..0x100088   .stub  — multiboot header + pre-paging stub
                                        (VMA == LMA, runs with paging off)
.phys 0x101000..           kernel — .text/.rodata/.data/.bss
                                        VMA = LMA + 0xC0000000 (0xC0101000..)
```

Early page tables (reserved in .bss by linker.ld, built by boot/boot.S by
physical address):

```
PD[0]    = early_low_pt   -> identity map 0..4 MiB
PD[768]  = early_low_pt   -> the SAME frames aliased at 0xC0000000..
                             (kernel window: 0xC0101000 -> phys 0x101000)
PD[1023] = early_pd       -> recursive (VMM maintains tables through it)
```

boot.S then sets `CR3 = early_pd(phys)`, enables `CR0.PG|WP`, installs the
(VMA) kernel stack from `stack_top` and calls `kmain` at its higher-half
address. The multiboot magic must survive the `CR0` dance — it is stashed in
`%esi` before `%eax` is clobbered.

## Bugs found (each caught by a real failure, not by reading)

1. **Missing `.multiboot` header.** The rewrite of boot.S for the higher half
   dropped the actual header definition (`multiboot_header: .long ...`) —
   only the constants `.include` survived. The build script's header check
   caught it: `no checksum-valid multiboot v1 header in image`.

2. **`.section` without ALLOC flags.** Bare `.section .stub` in gas defaults
   to a *non-alloc* section, so ld dropped the stub from the load image: the
   ELF had an empty orphan PT_LOAD at 0 and no PT_LOAD covered the multiboot
   header at all. Fix: `.section .multiboot, "ax", @progbits` and
   `.section .stub, "ax", @progbits`.

3. **EAX clobbered by the CR0 dance.** `mov %cr0, %eax / or / mov %eax, %cr0`
   destroys EAX — which still held the multiboot magic that is pushed as
   kmain's first argument. Saved magic in `%esi` first.

4. **Non-uniform phys<->virt delta.** The stub occupies
   `0x100000..0x100088`, so the kernel's LMA base is NOT 0x100000. Worse, a
   hand-chained `AT(LOADADDR(prev) + SIZEOF(prev))` drifts out of sync with
   the *aligned* VMA the linker actually assigns (rodata VMA got aligned to
   32 bytes, the LMA did not → delta `0xC000000D` for rodata/data). Every
   access through the 0xC0000000 window then lands 13 bytes off → immediate
   triple fault (QEMU `-d int,cpu_reset` showed the first #PF/#GPF in BIOS
   territory, `CR2=f000ff53`). Fixes, now load-bearing:
   - `KERNEL_LMA = ALIGN(LOADADDR(.stub) + SIZEOF(.stub), 4096)` and
     `KERNEL_VIRT = KERNEL_LMA + V2P` — kernel starts on the first 4 KiB
     page after the stub, on BOTH sides;
   - each section's LMA is derived from its own VMA:
     `AT(ADDR(.text) - V2P)` etc. — the delta is exactly V2P by construction,
     no matter what alignment ld inserts.

## Verification (all live, QEMU 9.2.0)

- ELF invariants (build_direct.sh + readelf): every kernel PT_LOAD has
  `PhysAddr = VirtAddr - 0xC0000000` exactly; entry `0x10000c`; checksum-
  valid multiboot header at file offset 0x1000.
- Smoke test PASSED: banner + `phase 2: higher-half kernel running at
  0xC0101000 (phys 1 MiB)`, 32736 frames, paging ON, no panic.
- `-DLINDOWS_FAULT_TEST` (#UD): clean PANIC, `eip = 0xC010218C`,
  stack `0xC0111FF0` — both visibly higher-half VMAs.
- `-DLINDOWS_PFAULT_TEST`: clean PANIC, `cr2 = 0x40000000` (injected address).
- PMM reserves the kernel image via the new `_kphys_start/_kphys_end` LMA
  symbols (the old `_kstart/_kend` are VMAs now — using them for physical
  bookkeeping would reserve the wrong frames).
