/*
 * LINDOWS kernel entry — phase 1.
 * Freestanding C. Prints a banner to the VGA text buffer.
 * Compiled with i686-elf-gcc -ffreestanding -nostdlib.
 */
#include "gen/profile_gen.h"

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

/* VGA text-mode framebuffer lives at physical 0xB8000. */
#define VIDEO_MEM ((volatile u16*)0xB8000)
#define COLS      80

static volatile u16* vmem = VIDEO_MEM;

static void putc(char c, u8 color) {
    static u32 col = 0;
    if (c == '\n') { col = (col / COLS + 1) * COLS; return; }
    vmem[col++] = (u16)((u16)color << 8) | (u16)c;
    if (col >= COLS * 25) col = 0;
}

static void puts(const char* s, u8 color) {
    while (*s) putc(*s++, color);
}

void kmain(void) {
    puts("LINDOWS\r\n", 0x07);          /* light grey on black */
    puts("profile: ", 0x0B);           /* cyan — replaced by active profile */
    puts(PROFILE_NAME, 0x0B);
    puts("\r\n", 0x07);
    /* Phase 1: halt. Real kernel will init GDT/IDT/paging here. */
    for (;;) { __asm__ volatile("hlt"); }
}
