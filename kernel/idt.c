/*
 * LINDOWS Phase 2 — IDT + PIC + PIT (timer IRQ) implementation.
 */
#include "idt.h"
#include "ports.h"

#define IDT_SIZE 256

/* Zeroed => not-present. BSS is zero-filled by GRUB, so this starts clean,
 * which means any vector we don't explicitly install raises #GP. */
static struct idt_entry idt[IDT_SIZE];
static struct idt_ptr   idt_register;

volatile u32 pit_ticks = 0;

/* LAPIC / IOAPIC MMIO base addresses (PC-standard defaults). */
#define LAPIC_BASE    0xFEE00000u
#define IOAPIC_BASE   0xFEC00000u

/* LAPIC register offsets (byte offsets from LAPIC_BASE). */
#define LAPIC_SVR     0x0F0
#define LAPIC_LINT0   0x350
#define LAPIC_EOI     0x0B0
#define LAPIC_TPR     0x080   /* Task Priority Register                 */

/*
 * Timer ISR. Runs at ring 0 with IF cleared (interrupt gate). We count ticks
 * and acknowledge the PIC so the next edge can be delivered. EOI is done here
 * to reuse the proven outb() path; isr_timer (boot/isr.S) handles pusha/popa/iret.
 */
void __attribute__((noinline)) timer_tick_handler(void) {
    pit_ticks++;
    /* LAPIC EOI — signals end of interrupt to the IOAPIC path. */
    *(volatile u32 *)(LAPIC_BASE + LAPIC_EOI) = 0;
    /* 8259A EOI — in case the interrupt came the legacy-PIC route. */
    outb(PIC_MASTER_CMD, 0x20);
}

static void idt_set_gate(u8 n, void (*handler)(void), u16 selector, u8 type_attr) {
    idt[n].offset_low  = (u16)((u32)handler);
    idt[n].selector    = selector;
    idt[n].zero        = 0;
    idt[n].type_attr   = type_attr;
    idt[n].offset_high = (u16)((u32)handler >> 16);
}

/* Remap the master/slave 8259A to vectors [base..base+7] and [base+8..base+15]. */
static void pic_remap(u8 vector_base) {
    outb(PIC_MASTER_CMD, 0x11);   /* ICW1: cascade, edge, expect ICW4   */
    outb(PIC_SLAVE_CMD,  0x11);
    outb(PIC_MASTER_DATA, vector_base);
    outb(PIC_SLAVE_DATA,  (u8)(vector_base + 8));
    outb(PIC_MASTER_DATA, 0x04);   /* master cascades on IR2            */
    outb(PIC_SLAVE_DATA,  0x02);   /* slave ID                          */
    outb(PIC_MASTER_DATA, 0x01);   /* ICW4: 8086 mode                   */
    outb(PIC_SLAVE_DATA,  0x01);
    outb(PIC_MASTER_DATA, 0xFD);   /* OCW1 master: unmask only IRQ0     */
    outb(PIC_SLAVE_DATA,  0xFF);   /* OCW1 slave: mask everything       */
}

/*
 * Ensure the LAPIC is enabled and the IOAPIC redirection table routes
 * IRQ0 (PIT) to our timer vector.  On real hardware SeaBIOS typically does
 * this, but QEMU's emulated pc machine leaves the IOAPIC entries unconfigured,
 * so the PIT's edge never reaches the CPU.
 */
static void apic_ioapic_setup(void) {
    /* 1. Enable LAPIC via IA32_APIC_BASE MSR (bit 11). */
    u32 lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0x1B));
    lo |= (1u << 11);              /* set LAPIC Global Enable          */
    __asm__ volatile("wrmsr" :: "a"(lo), "d"(hi), "c"(0x1B));

    /* 2. Set LAPIC SVR: enable (bit 8) + spurious vector 0xFF.
     *    Clear TPR first to accept all interrupt priorities. */
    volatile u32 *lapic = (volatile u32 *)LAPIC_BASE;
    lapic[LAPIC_TPR / 4] = 0x00;   /* Task Priority = 0 (accept all)      */
    lapic[LAPIC_SVR / 4] = 0x1FF;   /* enable | spurious-vector 0xFF */

    /* 3. LVT LINT0 = ExtINT (delivery mode 0x5 = 101b) so legacy-PIC
     *    edges on INTR passthrough the LAPIC to the CPU. */
    lapic[LAPIC_LINT0 / 4] = (0x5u << 8); /* ExtINT, unmasked */

    /* 4. IOAPIC redirection table: set up entries for IRQ0 through IRQ10
     *    (common timer/PIC sources).  Low dword = vector + fixed edge unmasked;
     *    High dword = target APIC ID 0. */
    volatile u32 *io_idx = (volatile u32 *)IOAPIC_BASE;
    volatile u32 *io_dat = (volatile u32 *)(IOAPIC_BASE + 0x10);
    u8 __i;
    for (__i = 0; __i <= 10; __i++) {
        *io_idx = 0x10 + 2 * __i;
        *io_dat = (u32)IRQ_TIMER_VEC;
        *io_idx = 0x10 + 2 * __i + 1;
        *io_dat = 0x00000000u;
    }
}

/* Program PIT channel 0 to `hz` using mode 2 (rate generator). */
static void pit_program(u32 hz) {
    if (hz < 1) hz = 1;
    u32 divisor = 1193182 / hz;     /* PIT input clock = 1.193182 MHz */
    outb(0x43, 0x34);              /* channel 0, lobyte/hi, mode 2    */
    outb(0x40, (u8)(divisor & 0xFF));
    outb(0x40, (u8)((divisor >> 8) & 0xFF));
}

void idt_init(void) {
    pic_remap(PIC_VECTOR_BASE);
    apic_ioapic_setup();

    /* Install only the timer; all other vectors stay not-present (#GP). */
    idt_set_gate(IRQ_TIMER_VEC, isr_timer, 0x08, 0x8E); /* 0x8E = present, ring0, int gate */

    idt_register.limit = (u16)(sizeof(idt) - 1);
    idt_register.base  = (u32)&idt;
    __asm__ volatile("lidt %0" :: "m"(idt_register));

    pit_program(100);              /* 100 Hz periodic timer */
    __asm__ volatile("sti");       /* global interrupt enable now safe */
}
