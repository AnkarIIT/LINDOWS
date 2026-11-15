/*
 * LINDOWS Phase 2 — IDT + 8259A PIC + PIT (timer IRQ).
 *
 * Builds a 256-entry IDT, remaps the 8259A cascade so hardware IRQs no longer
 * overlap the CPU exception vectors (0..7), programs the PIT for a 100 Hz
 * tick, and unmasks ONLY IRQ0. Every vector 0..47 is installed with a stub in
 * boot/isr.S that routes through isr_dispatch(), so faults are diagnosed, not
 * silent triple-faults. Must run AFTER gdt_init(): the gates reference the
 * kernel code-segment selector 0x08.
 */
#ifndef LINDOWS_IDT_H
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

/* Exception name table (0..31), filled by idt.c. */
extern const char* const exception_name[32];

/* C entry point for every gate (called from boot/isr.S common tails).
 * `regs` points at the uniform frame: regs[0..7]=EDI..EAX, [8]=error code,
 * [9]=EIP, [10]=CS, [11]=EFLAGS. */
void isr_dispatch(u32 vector, u32* regs);

/* PIT tick counter, incremented from sched_tick (isr32 path). */
extern volatile u32 pit_ticks;

/* Diagnostics counter for unexpected/spurious IRQs. */
extern volatile u32 unexpected_irqs;

/* Install IDT, remap + program PIC/PIT, enable IRQs. */
void idt_init(void);

#endif /* LINDOWS_IDT_H */
