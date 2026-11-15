/* 
 * LINDOWS Phase 3 — 64-bit long-mode C main
 * The Limine bootloader loads the ELF64 kernel and jumps to _start
 * in long mode with paging already enabled (CR0.PG, CR4.PAE, EFER.LME).
 * This function initializes the console, prints the banner, and loops forever.
 *
 * CPU model: QEMU TCG with -cpu max (no SVM/VMX needed).
 * No OVMF/UEFI — BIOS/SeaBIOS MBR boot only.
 * Long mode + paging guaranteed active by the Limine loader.
 */

#include "types.h"
#include "ports.h"
#include "console.h"

/* kmain64 — the main C function for long mode.
 * Called by _start in boot/boot64.S after the stack is set up.
 * The linker script sets the ELF entry point to _start in boot64.S,
 * which calls this function. */
void kmain64(void)
{
        /* Initialize the serial console (COM1 at 0x3F8, 115200 baud, 8N1). */
        console_init();

        /* Print the "LINDOWS long mode" banner via the serial/console port.
         * Since we're in long mode with paging on, this is the milestone-1
         * proof that we successfully transitioned to 64-bit mode.
         */
        console_puts("LINDOWS long mode — powered by x86_64 + Limine v12.10.0\n", CON_COLOR_WHITE);

        /* hlt loop — bring up PMM, VMM, heap, scheduler in later phases. */
        for (;;) __asm__ volatile("hlt");
}