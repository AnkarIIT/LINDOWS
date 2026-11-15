/*
 * LINDOWS scheduler implementation — see sched.h for the design.
 * All the stack-switching mechanics live in the isr32 stub; this file is
 * pure policy: who runs next, when stacks are born and recycled.
 */
#include "sched.h"
#include "idt.h"
#include "ports.h"
#include "heap.h"
#include "panic.h"

#define SCHED_MAX_TASKS  8
#define SCHED_QUANTUM    1    /* ticks per turn (10 ms @ 100 Hz)      */
#define TASK_STACK_SIZE  4096

static task_t tasks[SCHED_MAX_TASKS];          /* tasks[0] = kmain */
static task_t* current = &tasks[0];
static u32 nswitches = 0;
static u32 quantum_spent = 0;                  /* ticks on current slice */
static void* pending_free_stack = 0;           /* freed on next tick */

task_t* sched_current(void) { return current; }
u32 sched_switches(void)    { return nswitches; }

void sched_init(void) {
    tasks[0].id    = 0;
    tasks[0].state = TASK_RUNNING;   /* kmain IS task 0, running right now */
    for (u32 i = 1; i < SCHED_MAX_TASKS; i++)
        tasks[i].state = TASK_FREE;
}

/* Round-robin scan after the current slot. READY only: another task is
 * never RUNNING (only `current` is), and FREE/EXITED never run. */
static task_t* pick_next(void) {
    for (u32 i = 1; i < SCHED_MAX_TASKS; i++) {
        task_t* t = &tasks[(current->id + i) % SCHED_MAX_TASKS];
        if (t->state == TASK_READY) return t;
    }
    return 0;
}

u32 sched_tick(u32 frame) {
    /* A stack freed from itself is use-after-free by definition; dying
     * tasks defer the kfree to the next tick, which runs on a live stack. */
    if (pending_free_stack) {
        kfree(pending_free_stack);
        pending_free_stack = 0;
    }

    pit_ticks++;
    outb(PIC_MASTER_CMD, 0x20);   /* EOI exactly once, before any switch */

    current->runs++;
    if (++quantum_spent < SCHED_QUANTUM) return frame;  /* slice not spent */
    quantum_spent = 0;

    task_t* next = pick_next();
    if (!next) return frame;      /* nobody else runnable: keep running */

    if (current->state == TASK_EXITED) {
        pending_free_stack = current->stack_base;
        current->stack_base  = 0;
        current->state       = TASK_FREE;
    } else {
        current->state = TASK_READY;
    }
    current->saved_esp = frame;

    current = next;
    next->state = TASK_RUNNING;
    nswitches++;
    return next->saved_esp;
}

void task_spawn(void (*entry)(void)) {
    for (u32 i = 1; i < SCHED_MAX_TASKS; i++) {
        if (tasks[i].state != TASK_FREE) continue;

        u8* stack = kmalloc(TASK_STACK_SIZE);
        if (!stack) panic("sched: cannot allocate task stack", i, 0);

        /* Synthetic resume frame at the very top of the stack: the first
         * popa+add+iret on this task enters `entry` with IF=1 (0x202). */
        u32* f = (u32*)(stack + TASK_STACK_SIZE - 48);
        for (u32 k = 0; k < 12; k++) f[k] = 0;
        f[9]  = (u32)entry;    /* EIP                            */
        f[10] = 0x08;          /* CS: kernel code                */
        f[11] = 0x202;         /* EFLAGS: IF=1, reserved bit 1   */

        tasks[i].id         = i;
        tasks[i].saved_esp  = (u32)f;
        tasks[i].stack_base = stack;
        tasks[i].runs       = 0;
        tasks[i].state      = TASK_READY;
        return;
    }
    panic("sched: task table full", 0, 0);
}

void task_exit(void) {
    current->state = TASK_EXITED;
    /* hlt until the next tick switches away from us for good; with the
     * EXITED state set, pick_next() will never choose this task again. */
    for (;;) __asm__ volatile("hlt");
}
