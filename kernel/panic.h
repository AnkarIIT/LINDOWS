/*
 * LINDOWS panic — last-ditch fault handler.
 */
#ifndef LINDOWS_PANIC_H
#define LINDOWS_PANIC_H

#include "types.h"

/* Print reason + vector + register dump (if regs != 0), then halt forever.
 * `regs` is the interrupt frame pointer handed to isr_dispatch; pass 0 for
 * non-interrupt panics. */
void panic(const char* msg, u32 vector, const u32* regs);

#endif /* LINDOWS_PANIC_H */
