/*
 * LINDOWS kernel entry - phase 2 + runtime profile selection.
 * Freestanding C. Prints a banner to the VGA text buffer AND to COM1 serial,
 * so the boot can be verified headlessly with `qemu -serial stdio`.
 * Compiled with i686-elf-gcc -ffreestanding -nostdlib.
 *
 * At boot we offer a short (3s) serial profile picker:
 *   1 = minimal, 2 = developer, 3 = gamer. On timeout we default to minimal.
 * The selected profile is printed and (cosmetically) drives the boot banner.
 */
#include "gen/profile_gen.h"
#include "types.h"
#include "ports.h"
#include "gdt.h"
#include "idt.h"

/* VGA text-mode framebuffer lives at physical 0xB8000. */
#define VIDEO_MEM ((volatile u16*)0xB8000)
#define COLS      80

/* COM1 */
#define COM1 0x3F8

static volatile u16* vmem = VIDEO_MEM;

static void serial_init(void) {
    outb(COM1 + 1, 0x00);   /* no interrupts            */
    outb(COM1 + 3, 0x80);   /* DLAB on                  */
    outb(COM1 + 0, 0x01);   /* divisor low  = 1 -> 115200 */
    outb(COM1 + 1, 0x00);   /* divisor high = 0         */
    outb(COM1 + 3, 0x03);   /* 8N1                      */
    outb(COM1 + 2, 0xC7);   /* enable + clear FIFOs     */
    outb(COM1 + 4, 0x03);   /* DTR | RTS | OUT2         */
}

static void serial_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20))   /* wait while THR not empty */
        ;
    outb(COM1, (u8)c);
}

static int serial_has_data(void) {
    return (inb(COM1 + 5) & 0x01) != 0;
}

/* Block until a byte arrives or `timeout_ms` elapses (using the 100 Hz
 * PIT tick). Returns 0 on timeout. */
static char serial_getc_timeout(u32 timeout_ms) {
    u32 target = pit_ticks + (timeout_ms / 10) + 1;
    while (!serial_has_data()) {
        if (pit_ticks >= target) return 0;
        __asm__ volatile("hlt");
    }
    return (char)inb(COM1);
}

/* Write to both VGA text memory and serial so a headless run is verifiable. */
static void putc(char c, u8 color) {
    static u32 col = 0;
    if (c == '\n') { col = (col / COLS + 1) * COLS; }
    else           { vmem[col++] = (u16)((u16)color << 8) | (u16)c; }
    if (col >= COLS * 25) col = 0;
    if (c == '\r') return;              /* CR is meaningless on a serial line */
    serial_putc(c);
}

static void puts(const char* s, u8 color) {
    while (*s) putc(*s++, color);
}

/* Decimal printer for the tick counter; buf is big enough for u32 max. */
static void print_u32(u32 n) {
    char buf[12];
    u32 i = sizeof(buf);
    buf[--i] = 0;
    if (n == 0) buf[--i] = '0';
    while (n > 0) { buf[--i] = (char)('0' + (n % 10)); n /= 10; }
    puts(&buf[i], 0x0F);
}

static const char* profile_name_for(char c) {
    if (c == '2') return "developer";
    if (c == '3') return "gamer";
    return "minimal";
}

void kmain(void) {
    serial_init();
    gdt_init();                       /* Phase 2: install protected-mode GDT */
    idt_init();                       /* Phase 2: IDT + PIC + 100 Hz timer   */

    /* ---- runtime profile selection (3s serial timeout -> default) ---- */
    puts("LINDOWS boot\r\n", 0x07);
    puts("select profile: 1=minimal 2=developer 3=gamer (timeout 3s -> minimal)\r\n", 0x0B);
    puts("choice: ", 0x0B);
    char choice = serial_getc_timeout(3000);
    if (choice == 0) {
        puts("(timeout)\r\n", 0x08);
    } else {
        putc(choice, 0x0F);
        puts("\r\n", 0x07);
    }
    const char* profile = profile_name_for(choice);

    puts("profile: ", 0x0B);
    puts(profile, 0x0B);
    puts("\r\n", 0x07);
    puts("boot ok: COM1 serial up, VGA text up, timer IRQ live\r\n", 0x0A);
    puts("phase 2: GDT installed (32-bit PM, flat memory, code+data)\r\n", 0x0E);
    puts("phase 2: IDT + 100 Hz timer IRQ online\r\n", 0x09);

#ifdef LINDOWS_DEBUG
    u32 eflags;
    __asm__ volatile("pushf; pop %0" : "=r"(eflags));
    puts("debug: IF=", 0x0C);
    print_u32((eflags >> 9) & 1);
    puts(" t0=", 0x0C);
    print_u32(pit_ticks);
    puts("\r\n", 0x07);
#endif

    /* Phase 2 continues: paging, heap, scheduler land here. */
    for (;;) { __asm__ volatile("hlt"); }
}