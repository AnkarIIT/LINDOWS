/*
 * LINDOWS Phase 2 — IDT + 8259A PIC + PIT (timer IRQ).
 *
 * Design (kept deliberately minimal):
 *   - Legacy 8259A PIC is the ONLY interrupt delivery path. Remap master/slave
 *     to vectors 0x20..0x2F. APIC/IOAPIC bringup is deferred to Phase 4, where
 *     it belongs (CPUID checks, MMIO mapping via paging).
 *   - All 32 CPU exception vectors + all 16 IRQ vectors are installed, so any
 *     fault or unexpected IRQ produces a readable serial message instead of a
 *     silent triple-fault reboot.
 *   - Only IRQ0 (PIT, 100 Hz) is unmasked.
 */
#include "idt.h"
#include "ports.h"
#include "console.h"
#include "panic.h"

#define IDT_SIZE 256

/* Zeroed => not-present. BSS is zero-filled by GRUB/QEMU. We install every
 * vector 0..47 explicitly, so a not-present entry above 47 means the hardware
 * raised something we truly did not expect. */
static struct idt_entry idt[IDT_SIZE];
static struct idt_ptr   idt_register;

volatile u32 pit_ticks = 0;

/* Counters for diagnostics (visible via debugger / future debug shell). */
volatile u32 unexpected_irqs = 0;

void isr_dispatch(u32 vector, u32* regs) {
    if (vector < 32) {
        panic(exception_name[vector], vector, regs);
    }

    /* IRQ0 (vector 32) never reaches here: the isr32 stub in boot/isr.S
     * routes it through sched_tick() (tick count, EOI, context switch). */

    /* Unexpected but PIC-routed: log it, EOI it, keep running. Spurious IRQ7
     * lands here, for example. */
    unexpected_irqs++;
    console_puts("[irq] unexpected vector ", CON_COLOR_YELLOW);
    console_print_hex(vector, CON_COLOR_WHITE);
    console_puts("\r\n", CON_COLOR_WHITE);
    if (vector >= 0x28 && vector <= 0x2F) {
        outb(PIC_SLAVE_CMD, 0x20);            /* EOI slave + master */
    }
    outb(PIC_MASTER_CMD, 0x20);
}

/* Exception names (Intel SDM ch. 6). */
const char* const exception_name[32] = {
    "divide-by-zero (#DE)",
    "debug (#DB)",
    "non-maskable interrupt",
    "breakpoint (#BP)",
    "overflow (#OF)",
    "bound-range (#BR)",
    "invalid opcode (#UD)",
    "device not available (#NM)",
    "double fault (#DF)",
    "coprocessor segment (#CS)",
    "invalid TSS (#TS)",
    "segment not present (#NP)",
    "stack-segment fault (#SS)",
    "general protection fault (#GP)",
    "page fault (#PF)",
    "reserved (15)",
    "x87 FPU error (#MF)",
    "alignment check (#AC)",
    "machine check (#MC)",
    "SIMD fp exception (#XM)",
    "virtualization exception (#VE)",
    "control protection (#CP)",
    "reserved (22)", "reserved (23)", "reserved (24)", "reserved (25)",
    "reserved (26)", "reserved (27)", "reserved (28)",
    "reserved (29)", "reserved (30)", "reserved (31)",
};

static void idt_set_gate(u8 n, void (*handler)(void), u16 selector, u8 type_attr) {
    idt[n].offset_low  = (u16)((u32)handler);
    idt[n].selector    = selector;
    idt[n].zero        = 0;
    idt[n].type_attr   = type_attr;
    idt[n].offset_high = (u16)((u32)handler >> 16);
}

/* Remap the master/slave 8259A to vectors [base..base+7] and [base+8..base+15],
 * then mask everything except IRQ0 (PIT). Master mask 0xFE = bit0 clear. */
static void pic_init(u8 vector_base) {
    outb(PIC_MASTER_CMD, 0x11);   /* ICW1: cascade, edge, expect ICW4   */
    outb(PIC_SLAVE_CMD,  0x11);
    outb(PIC_MASTER_DATA, vector_base);
    outb(PIC_SLAVE_DATA,  (u8)(vector_base + 8));
    outb(PIC_MASTER_DATA, 0x04);   /* master cascades on IR2            */
    outb(PIC_SLAVE_DATA,  0x02);   /* slave ID                          */
    outb(PIC_MASTER_DATA, 0x01);   /* ICW4: 8086 mode                   */
    outb(PIC_SLAVE_DATA,  0x01);
    outb(PIC_MASTER_DATA, 0xFE);   /* OCW1: unmask ONLY IRQ0            */
    outb(PIC_SLAVE_DATA,  0xFF);   /* OCW1 slave: mask everything       */
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
    pic_init(PIC_VECTOR_BASE);

    /* Extern declarations for all 48 stubs in boot/isr.S. */
    #define D(n) extern void isr##n(void);
    D(0)D(1)D(2)D(3)D(4)D(5)D(6)D(7)D(8)D(9)D(10)D(11)D(12)D(13)D(14)D(15)
    D(16)D(17)D(18)D(19)D(20)D(21)D(22)D(23)D(24)D(25)D(26)D(27)D(28)D(29)D(30)D(31)
    D(32)D(33)D(34)D(35)D(36)D(37)D(38)D(39)D(40)D(41)D(42)D(43)D(44)D(45)D(46)D(47)
    #undef D

    static void (*const stubs[48])(void) = {
        isr0,isr1,isr2,isr3,isr4,isr5,isr6,isr7,isr8,isr9,
        isr10,isr11,isr12,isr13,isr14,isr15,isr16,isr17,isr18,isr19,
        isr20,isr21,isr22,isr23,isr24,isr25,isr26,isr27,isr28,isr29,
        isr30,isr31,isr32,isr33,isr34,isr35,isr36,isr37,isr38,isr39,
        isr40,isr41,isr42,isr43,isr44,isr45,isr46,isr47,
    };

    /* Interrupt gates (0x8E): present, ring 0, IF cleared on entry — correct
     * for both exceptions and IRQs. */
    for (u32 i = 0; i < 48; i++)
        idt_set_gate((u8)i, stubs[i], 0x08, 0x8E);

    idt_register.limit = (u16)(sizeof(idt) - 1);
    idt_register.base  = (u32)&idt;
    __asm__ volatile("lidt %0" :: "m"(idt_register));

    pit_program(100);              /* 100 Hz periodic timer */
    __asm__ volatile("sti");       /* global interrupt enable now safe */
}
