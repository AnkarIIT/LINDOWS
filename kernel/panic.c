/*
 * LINDOWS panic — last-ditch fault handler. Prints a register dump to VGA +
 * serial and halts. Called from isr_dispatch() for CPU exceptions and from
 * kernel code for "impossible" states.
 */
#include "panic.h"
#include "console.h"

void panic(const char* msg, u32 vector, const u32* regs) {
    __asm__ volatile("cli");   /* no further interrupts; we are already lost */

    console_puts("\r\n*** PANIC ***\r\n", CON_COLOR_RED);
    console_puts("reason : ", CON_COLOR_RED);
    console_puts(msg ? msg : "(none)", CON_COLOR_WHITE);
    console_puts("\r\nvector : ", CON_COLOR_RED);
    console_print_hex(vector, CON_COLOR_WHITE);

    if (regs) {
        /* Frame layout guaranteed by boot/isr.S — see the comment there. */
        static const char* rnames[8] = {
            "EDI", "ESI", "EBP", "ESP", "EBX", "EDX", "ECX", "EAX"
        };
        console_puts("\r\nerror  : ", CON_COLOR_RED);
        console_print_hex(regs[8], CON_COLOR_WHITE);
        console_puts("\r\neip    : ", CON_COLOR_RED);
        console_print_hex(regs[9], CON_COLOR_WHITE);
        console_puts("\r\ncs     : ", CON_COLOR_RED);
        console_print_hex(regs[10], CON_COLOR_WHITE);
        console_puts("\r\neflags : ", CON_COLOR_RED);
        console_print_hex(regs[11], CON_COLOR_WHITE);
        u32 cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        console_puts("\r\ncr2    : ", CON_COLOR_RED);
        console_print_hex(cr2, CON_COLOR_WHITE);   /* #PF faulting address */
        for (int i = 0; i < 8; i++) {
            console_puts("\r\n", CON_COLOR_RED);
            console_puts(rnames[i], CON_COLOR_RED);
            console_puts("      : ", CON_COLOR_RED);
            console_print_hex(regs[i], CON_COLOR_WHITE);
        }
    }
    console_puts("\r\nsystem halted.\r\n", CON_COLOR_RED);

    for (;;) { __asm__ volatile("hlt"); }
}
