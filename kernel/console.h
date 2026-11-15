/*
 * LINDOWS console — COM1 serial + VGA text output, shared by all of the
 * kernel. Extracted from main.c so panic()/isr_dispatch() can print too.
 */
#ifndef LINDOWS_CONSOLE_H
#define LINDOWS_CONSOLE_H

#include "types.h"

/* Standard VGA attribute bytes. */
#define CON_COLOR_WHITE  0x0F
#define CON_COLOR_GREY   0x08
#define CON_COLOR_YELLOW 0x0E
#define CON_COLOR_RED    0x0C
#define CON_COLOR_GREEN  0x0A
#define CON_COLOR_CYAN   0x0B

/* Init COM1 (115200 8N1). VGA needs no init. */
void console_init(void);

/* Write one char to VGA text buffer + COM1. Supports \n, scrolls at line 25. */
void console_putc(char c, u8 color);

/* Write a NUL-terminated string (each char in `color`). */
void console_puts(const char* s, u8 color);

/* Unsigned decimal. */
void console_print_u32(u32 n, u8 color);

/* Unsigned hex (0x-prefixed, 8 digits) — for register dumps. */
void console_print_hex(u32 n, u8 color);

#endif /* LINDOWS_CONSOLE_H */
