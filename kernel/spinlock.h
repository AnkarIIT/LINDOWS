/*
 * LINDOWS spinlocks — single-core critical sections (Phase 2).
 *
 * Honest design note: with ONE CPU, the only way another context can
 * corrupt a critical section is an interrupt pre-empting it. So the correct
 * single-core lock is "mask interrupts, flag held, unmask on release":
 *
 *   spin_lock:   save EFLAGS, cli, then claim. If it is ALREADY held while
 *                we run with IF=0, the holder can never make progress —
 *                that is a bug, so we panic with the lock's address instead
 *                of spinning into a frozen kernel. (On SMP this is where a
 *                real test-and-set spin loop goes — Phase 6.)
 *
 *   spin_unlock: clear the flag, then restore the SAVED EFLAGS with popfl —
 *                NOT a blind sti. This is what makes nesting correct: a
 *                lock taken inside an IRQ handler (IF already 0) releases
 *                back to IF=0, never re-enabling interrupts early.
 *
 * Rules: never hold across something that yields forever; never call
 * panic()/kmalloc-reentrantly from inside a critical section that itself
 * runs in the heap lock.
 */
#ifndef LINDOWS_SPINLOCK_H
#define LINDOWS_SPINLOCK_H

#include "types.h"
#include "panic.h"

typedef struct {
    volatile u32 held;         /* 0 = free, 1 = held                    */
    u32 saved_eflags;          /* flags captured at acquire (popfl'd)   */
} spinlock_t;

#define SPINLOCK_INIT { 0, 0 }

static inline void spin_lock(spinlock_t* l) {
    u32 eflags;
    __asm__ volatile("pushfl\n\tpopl %0\n\tcli" : "=r"(eflags) :: "memory");
    if (l->held)
        panic("spinlock: deadlock - held while IF=0 (no SMP spin yet)",
              (u32)l, 0);
    l->saved_eflags = eflags;
    l->held = 1;               /* no race: IF=0 from here on            */
}

static inline void spin_unlock(spinlock_t* l) {
    u32 f = l->saved_eflags;
    l->held = 0;
    __asm__ volatile("pushl %0\n\tpopfl" :: "r"(f) : "memory");
}

#endif /* LINDOWS_SPINLOCK_H */
