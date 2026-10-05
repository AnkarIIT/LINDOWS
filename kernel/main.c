/*
 * LINDOWS kernel entry — phase 1.
 * Freestanding C. Prints a banner to the VGA text buffer AND to COM1 serial,
 * so the boot can be verified headlessly with `qemu -serial stdio`.
 * Compiled with i686-elf-gcc -ffreestanding -nostdlib.
 */
#include "gen/profile_gen.h"

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

/* VGA text-mode framebuffer lives at physical 0xB8000. */
#define VIDEO_MEM ((volatile u16*)0xB8000)
#define COLS      80

/* COM1 */
#define COM1 0x3F8

static volatile u16* vmem = VIDEO_MEM;

static inline void outb(u16 port, u8 val) {
    __asm__ volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 r;
    __asm__ volatile("inb %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}

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

void kmain(void) {
    serial_init();

    puts("LINDOWS\r\n", 0x07);          /* light grey on black */
    puts("profile: ", 0x0B);           /* cyan — the active profile */
    puts(PROFILE_NAME, 0x0B);
    puts("\r\n", 0x07);

    puts("boot ok: COM1 serial up, VGA text up\r\n", 0x0A);
    puts("phase 1 complete. halting.\r\n", 0x07);

    /* Phase 1: halt. Real kernel will init GDT/IDT/paging here. */
    for (;;) { __asm__ volatile("hlt"); }
}