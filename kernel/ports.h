/*
 * LINDOWS x86 port I/O.
 * `Nd` lets GCC pass an immediate port (0-255) or %dx for the wider range.
 */
#ifndef LINDOWS_PORTS_H
#define LINDOWS_PORTS_H

#include "types.h"

static inline void outb(u16 port, u8 val) {
    __asm__ volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 r;
    __asm__ volatile("inb %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}

#endif /* LINDOWS_PORTS_H */
