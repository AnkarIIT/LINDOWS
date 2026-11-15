/*
 * LINDOWS panic — last-ditch fault handler (simplified for build).
 * Called from isr_dispatch() for CPU exceptions.
 * In this minimal build, we just print and halt — no inline assembly.
 */

#include "panic.h"
#include "console.h"

void panic(const char* msg, u32 vector, const u32* regs) {
    /* Use the console_putc/console_print_hex functions directly.
     * The register save format is guaranteed by boot/isr.S.
     */
    console_puts("\r\n*** PANIC ***\r\n", CON_COLOR_RED);

    if (msg) {
        console_puts("reason : ", CON_COLOR_RED);
        console_puts(msg, CON_COLOR_WHITE);
    } else {
        console_puts("reason : (none)", CON_COLOR_WHITE);
    }

    console_puts("vector : ", CON_COLOR_RED);
    console_print_hex(vector, CON_COLOR_WHITE);

    if (regs) {
        static const char* rnames[8] = {
            "EDI", "ESI", "EBP", "ESP", "EBX", "EDX", "ECX", "EAX"
        };
        console_puts("\r\nerror  : ", CON_COLOR_RED);
        console_print_hex(regs[8], CON_COLOR_WHITE);
        console_puts("\r\neip    : ", CON_COLOR_RED);
        console_print_hex(regs[9], CON_COLOR_WHITE);
        console_puts("\r\ncs     : ", CON_COLOR_RED);
        console_print_hex(regs[10], CON_COLOR_RED);
        console_puts("\r\neflags : ", CON_COLOR_RED);
        console_print_hex(regs[11], CON_COLOR_WHITE);
        console_puts("\r\ncr2    : ", CON_COLOR_RED);
        console_print_hex(regs[0], CON_COLOR_WHITE); /* simplified: just print regs[0] */
        for (int i = 0; i < 8; i++) {
            console_puts("\r\n", CON_COLOR_RED);
            console_puts(rnames[i], CON_COLOR_RED);
            console_puts("      : ", CON_COLOR_RED);
            console_print_hex(regs[i], CON_COLOR_RED);
        }
    }
    console_puts("\r\nsystem halted.\r\n", CON_COLOR_RED);

    for (;;) { __asm__ volatile("hlt"); }
}