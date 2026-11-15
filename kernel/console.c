/*
 * LINDOWS console implementation — VGA text buffer + COM1 serial mirror.
 * Everything the kernel prints goes through here, so a headless QEMU run
 * (-serial file:...) captures the full boot log including panics.
 */
#include "console.h"
#include "ports.h"

/* VGA text-mode framebuffer at physical 0xB8000, 80x25, u16 char+attr. */
#define VIDEO_MEM ((volatile u16*)0xB8000)
#define COLS      80
#define ROWS      25

/* COM1 */
#define COM1 0x3F8

static volatile u16* vmem = VIDEO_MEM;
static u32 cur_col = 0;
static u32 cur_row = 0;

void console_init(void) {
    outb(COM1 + 1, 0x00);   /* no interrupts              */
    outb(COM1 + 3, 0x80);   /* DLAB on                    */
    outb(COM1 + 0, 0x01);   /* divisor low  = 1 -> 115200 */
    outb(COM1 + 1, 0x00);   /* divisor high = 0           */
    outb(COM1 + 3, 0x03);   /* 8N1                        */
    outb(COM1 + 2, 0xC7);   /* enable + clear FIFOs       */
    outb(COM1 + 4, 0x03);   /* DTR | RTS | OUT2           */
}

static void serial_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20))   /* wait while THR not empty */
        ;
    outb(COM1, (u8)c);
}

/* Scroll the VGA buffer up one row and blank the last row. */
static void scroll(void) {
    for (u32 i = 0; i < (ROWS - 1) * COLS; i++)
        vmem[i] = vmem[i + COLS];
    for (u32 i = (ROWS - 1) * COLS; i < ROWS * COLS; i++)
        vmem[i] = (u16)(CON_COLOR_GREY << 8);
}

void console_putc(char c, u8 color) {
    if (c == '\n') {
        cur_col = 0;
        cur_row++;
    } else if (c == '\r') {
        cur_col = 0;
    } else {
        vmem[cur_row * COLS + cur_col] = (u16)((u16)color << 8) | (u16)c;
        cur_col++;
        if (cur_col >= COLS) { cur_col = 0; cur_row++; }
    }
    if (cur_row >= ROWS) { scroll(); cur_row = ROWS - 1; }
    serial_putc(c);
}

void console_puts(const char* s, u8 color) {
    while (*s) console_putc(*s++, color);
}

void console_print_u32(u32 n, u8 color) {
    char buf[11];
    u32 i = sizeof(buf);
    buf[--i] = 0;
    if (n == 0) buf[--i] = '0';
    while (n > 0) { buf[--i] = (char)('0' + (n % 10)); n /= 10; }
    console_puts(&buf[i], color);
}

void console_print_hex(u32 n, u8 color) {
    static const char hexd[] = "0123456789ABCDEF";
    console_puts("0x", color);
    for (int shift = 28; shift >= 0; shift -= 4)
        console_putc(hexd[(n >> shift) & 0xF], color);
}
