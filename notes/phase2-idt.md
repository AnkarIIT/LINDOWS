# Phase 2 — IDT + exceptions + panic — in-session, Nov 2026

## Goal
Make every CPU fault a *readable serial message* instead of a silent
triple-fault reboot, before paging work begins (paging is the most
fault-prone phase; debugging it blind is unacceptable).

## What changed (refactor of the pre-refactor checkpoint)

- `boot/isr.S`   — rewritten: uniform stubs for ALL 48 vectors (32 exceptions
  + 16 IRQs). Each stub: [fake error code] → pusha → push vector → jmp common.
  Common tails call C `isr_dispatch(vector, regs)` then unwind + iret.
  Uniform frame: regs[0..7]=EDI..EAX, regs[8]=error code, regs[9]=EIP,
  regs[10]=CS, regs[11]=EFLAGS. Error-code vectors (8,10..14,17,21) and
  no-error vectors normalize to the same shape.
- `kernel/idt.c` — simplified to PIC-ONLY delivery: the entire
  apic_ioapic_setup() block (LAPIC SVR/LINT0/EOI + IOAPIC redirections for
  IRQ0..10, all pointing at the timer vector while the legacy PIC was also
  remapped) was removed. APIC returns in Phase 4 with CPUID checks and
  paging-aware MMIO. Master PIC mask is now 0xFE (ONLY IRQ0 unmasked — the
  old 0xFD unmasked the keyboard IRQ1 → first keypress = triple fault).
  isr_dispatch(): exceptions → panic(); vec 0x20 → pit_ticks++ + EOI;
  anything else → logged + EOI (catches spurious IRQ7 gracefully).
- `kernel/console.c/.h` — extracted serial+VGA output from main.c so panic
  can print; added real scrolling (old code wrapped at 80x25 and clobbered
  line 0) and console_print_hex for dumps.
- `kernel/panic.c/.h` — new: reason + vector + error code + EIP + CS +
  EFLAGS + 8 GPRs dump, then cli + hlt forever.
- `kernel/main.c` — kmain(u32 magic, u32 mbi): the multiboot info pointer is
  now SAVED (boot.S pushes EAX/EBX before the call) — paging will need the
  memory map. The 3-second runtime profile picker was REMOVED from the boot
  path: build-time PROFILE_NAME is the single source of truth. The timer
  claim is now EARNED: wait_ticks(t0+5) proves 5 real PIT ticks before
  printing "timer verified". Boot delay went 3 s → ~50 ms.
  LINDOWS_FAULT_TEST build flag: executes `ud2` to verify the exception
  path end-to-end (a plain null-deref does NOT fault here — no paging yet).
- `boot/boot.S` — validates EAX == 0x2BADB002, passes magic + mbi to kmain.
- `tools/smoke_test.sh` — new: the automated boot test (build → headless
  QEMU 8 s → assert serial log: banner, "timer verified", "boot ok",
  no "PANIC").
- `tools/build_direct.sh` — new objects (console, panic), env-overridable
  ROOT, correct exit code. Makefile mirrors it.
- `.gitattributes` — LF everywhere (the checkpoint commit showed Windows
  CRLF conversion warnings on every source file).

## Bug the smoke test caught during bring-up
First version of isr.S unwound the stack off by one dword (forgot to drop
the pushed vector before popa) → immediate #GP in the stub → caught by the
new PANIC dump with exact EIP + error code, instead of an invisible
triple-fault. Exactly the failure mode this work exists to eliminate.

## Verification (QEMU 9.2.0, vendored at C:\OS\AI-OS\vendor\qemu)
- Normal build: SMOKE TEST PASSED
  (banner, "timer verified: 5 ticks in ~50ms", "boot ok", no panic).
- -DLINDOWS_FAULT_TEST build: clean PANIC dump —
  reason "invalid opcode (#UD)", vector 0x6, EIP 0x10091A (the ud2),
  full GPR dump, "system halted."
- kernel/lindows.bin: 10640 bytes, ELF32 i386, entry 0x10000c,
  multiboot header + checksum verified by build script.

## Next
Phase 2 continues: physical page allocator from the multiboot memory map
(now saved in mb_info), identity map + higher-half, enable paging, then
heap + scheduler.
