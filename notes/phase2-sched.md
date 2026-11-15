# Phase 2 — Pre-emptive round-robin scheduler + spinlocks

Goal: pre-emption on the PIT we already run at 100 Hz, round-robin over
kernel tasks, and a single-core locking primitive for shared state (the
heap got it first). Files: kernel/spinlock.h, kernel/sched.{h,c},
boot/isr.S (IRQ0 stub), kernel/idt.c (timer branch moved out),
kernel/heap.c (locks), kernel/main.c (demo tasks).

## Where the switch happens

The IRQ0 (vector 32) stub is now its own path, NOT isr_dispatch:

```
isr32: push $0; pusha
       mov %esp, %eax        # frame = &EDI
       push %eax; call sched_tick
       pop %ecx              # cdecl arg cleanup
       mov %eax, %esp        # ADOPT the returned frame (ours or next task's)
       popa; add $4; iret
```

sched_tick() counts pit_ticks, sends the PIC EOI (exactly once, before any
switch), then round-robins. Returning the incoming frame = no switch;
returning another task's saved_esp = context switch with one instruction.

Frame layout (ascending): pusha block [EDI ESI EBP ESP(old) EBX EDX ECX
EAX], fake-error slot, then EIP/CS/EFLAGS for iret. saved_esp points at
EDI; task_spawn builds the same synthetic frame on a kmalloc'd 4 KiB stack
(entry EIP, CS=0x08, EFLAGS=0x202).

## Bugs found (all caught live, none by inspection)

1. **#GP at the first timer tick, error 0x244.** isr32 computed the frame
   pointer with `lea 4(%esp), %eax` — copied from the generic stub tails,
   where it is correct ONLY because those stubs push `$vector` after
   `pusha` (so esp sits on the vector slot and +4 lands on EDI). isr32
   pushes no vector, so `+4` landed on ESI: every resumed frame was one
   slot high, and `iret` loaded EFLAGS (0x244 = PF|ZF|IF) into CS.
   QEMU `-d int` gave the proof: entry SP 0xC0112FCC, fault SP 0xC0112FD0
   = frame+36 — exactly the shifted layout. Fix: `mov %esp, %eax`.

2. **Demo tasks printed per-iteration, not per-slice.** First boot printed
   `<A><A><A><A><A><B><B><B><B><B>` with 3 switches: each task burned all
   5 prints inside its first slice, which proves pre-emption but NOT
   round-robin. Fix: each task sleeps one tick after printing (paces one
   token per slice). Serial then shows strict `<A><B><A><B>...` over 15
   switches (3 per cycle: kmain→A→B→kmain, 5 cycles).

3. **Self-deadlock caught at compile-review:** kcalloc briefly took the
   heap lock and then called kmalloc, which takes the same lock — the
   deadlock-detector spin_lock would have panicked. Wrappers take no lock
   of their own; kmalloc/kfree lock internally.

## Design notes

- **Spinlock = interrupt-masking lock.** Single core: spin_lock saves
  EFLAGS + cli, then claims; if already held under IF=0 the holder can
  never run, so it panics ("deadlock") instead of spinning forever.
  spin_unlock restores the SAVED flags with popfl — not a blind sti — so
  a lock taken inside an IRQ handler (IF already 0) releases correctly.
  Real test-and-set atomics wait for SMP (Phase 6).
- **Stack recycling is deferred one tick.** A dying task's stack holds the
  very frame the tick ISR is running on; kfree from inside it would be a
  use-after-free. sched_tick frees the previous tick's dead stack at its
  start (running on a live stack), and only then switches.
- **kmain is task 0 and the idle context.** It spawns the demo tasks, hlt
  loops, and wakes every tick; once both tasks exit, no READY task
  remains, switching stops, and kmain prints the verified line.
- Quantum = 1 tick (10 ms). A per-profile tunable later (the profile story:
  e.g. 'developer' longer slices, 'gamer' low latency).

## Verification (all live, QEMU 9.2.0)

- Boot log: `<A><B><A><B><A><B><A><B><A><B>` then
  `sched verified: A=5 B=5, 15 pre-emptive switches (round-robin)`.
- tools/smoke_test.sh now FAILs if: "sched verified" missing, fewer than
  5 A or B tokens, or no strict `<A><B>` alternation (a working round-robin
  is the only way to produce it).
- Regression: -DLINDOWS_PFAULT_TEST still panics cleanly with
  cr2 = 0x40000000 (the scheduler does not disturb the fault path).
- Phase 2 is now COMPLETE: GDT, IDT/exceptions, PMM, paging, higher-half
  link, heap, scheduler — all machine-verified.
