/*
 * LINDOWS Phase 2 — IDT + 8259A PIC + PIT (timer IRQ).
 *
 * Builds a 256-entry IDT, remaps the 8259A cascade so hardware IRQs no longer
 * overlap the CPU exception vectors (0..7), programs the PIT for a 100 Hz
 * tick, and unmasks the timer line (IRQ0). Must run AFTER gdt_init(), because
 * its interrupt gates reference the kernel code-segment selector 0x08.
 */
#ifndef LINDOWS_IDT_H
#define LINDATIONS_IDT_H
#define LINDOWS_IDT_H

#include "types.h"

/* 32-bit interrupt gate descriptor (Intel SDM 3.6.2 / OSDev wiki). */
struct idt_entry {
    u16 offset_low;   /* bits  0..15 of ISR offset            */
    u16 selector;     /* GDT code-segment selector (0x08)     */
    u8  zero;         /* reserved, must be 0                  */
    u8  type_attr;    /* P(1) DPL(2) S(0) gate-type(4) -> 0x8E */
    u16 offset_high;  /* bits 16..31 of ISR offset            */
} __attribute__((packed));

struct idt_ptr {
    u16 limit;
    u32 base;
} __attribute__((packed));

/* 8259A I/O ports. */
#define PIC_MASTER_CMD   0x20
#define PIC_MASTER_DATA  0x21
#define PIC_SLAVE_CMD    0xA0
#define PIC_SLAVE_DATA   0xA1

/* After remap, IRQs land at vectors 0x20..0x2F (32..47). */
#define PIC_VECTOR_BASE  0x20
#define IRQ_TIMER        0
#define IRQ_TIMER_VEC    (PIC_VECTOR_BASE + IRQ_TIMER)

/* Assembly ISR entry for the timer (see boot/isr.S). */
extern void isr_timer(void);

/* PIT tick counter, incremented from the timer ISR. */
extern volatile u32 pit_ticks;

/* Install IDT, remap + program PIC/PIT, enable IRQs. */
void idt_init(void);

#endif /* LINDOWS_IDT_H */
