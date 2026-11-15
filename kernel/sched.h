/*
 * LINDOWS scheduler — pre-emptive round-robin over kernel tasks (Phase 2).
 *
 * Everything is kernel-mode for now (userland is Phase 5). A "task" is a
 * 4 KiB kernel stack (kmalloc'd) plus a saved resume frame. Pre-emption
 * lives in boot/isr.S: the IRQ0 (PIT) stub saves the full GPR frame, calls
 * sched_tick(frame), and irets through WHATEVER frame pointer sched_tick
 * returns — ours (no switch) or the next task's (switch).
 *
 * Frame layout (matches the stubs; ascending addresses):
 *   [0..7]  EDI ESI EBP ESP(old) EBX EDX ECX EAX   (pusha block)
 *   [8]     fake error-code slot (popped by the stub's `add $4`)
 *   [9..11] EIP, CS, EFLAGS                       (iret pops these)
 * saved_esp points at EDI; resuming = `mov esp, saved_esp; popa; add 4; iret`.
 * task_spawn builds exactly this synthetic frame on the new stack.
 *
 * Task 0 is kmain (the idle context). TASK state machine:
 *   FREE -> READY (spawn) -> RUNNING (switch-to) -> READY | EXITED
 *   EXITED -> (stack freed deferred to next tick) -> FREE
 * A task exits by calling task_exit(): it sets EXITED and hlt's; the next
 * tick switches away and recycles its stack (freed on the NEXT tick's
 * stack, never from inside the dying stack itself).
 *
 * Quantum: 1 tick (10 ms @ 100 Hz) for Phase 2 — tunable per profile later
 * (e.g. a longer-slice 'developer' profile vs low-latency 'gamer').
 */
#ifndef LINDOWS_SCHED_H
#define LINDOWS_SCHED_H

#include "types.h"

typedef struct task {
    u32   saved_esp;    /* resume frame pointer (see boot/isr.S)   */
    u32   id;           /* index in the task table                 */
    u32   state;        /* task_state below                        */
    u32   runs;         /* ticks while scheduled (diagnostics)     */
    void* stack_base;   /* kmalloc'd 4 KiB stack, 0 for task 0     */
} task_t;

enum { TASK_FREE = 0, TASK_READY, TASK_RUNNING, TASK_EXITED };

/* Call once after heap_init(). Marks the current context (kmain) as task 0;
 * no tasks exist until task_spawn(). */
void sched_init(void);

/* IRQ0 pre-emption entry (called from isr32 stub with interrupts off).
 * Counts the tick, sends the PIC EOI, then round-robins. Returns the frame
 * pointer the stub must iret through. */
u32 sched_tick(u32 frame);

/* Create a task that starts at `entry` on its own stack (first slot with
 * TASK_FREE). Panics if the table is full or a stack cannot be allocated. */
void task_spawn(void (*entry)(void));

/* Exit the current task; never returns. */
void task_exit(void) __attribute__((noreturn));

/* Diagnostics. */
u32 sched_switches(void);

#endif /* LINDOWS_SCHED_H */
