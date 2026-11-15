/*
 * LINDOWS Phase 2 — assembly ISR stubs (GNU as, .code32).
 *
 * An interrupt gate expects its handler to end in `iret` (to pop the
 * EFLAGS/CS/EIP the CPU pushed on entry). A C function returns with `ret`,
 * which would misalign the stack, so each IRQ has a tiny asm prologue that
 * saves/restores the GPRs and irets on exit.
 */
.code32

.section .text
.global isr_timer
.extern timer_tick_handler

/* IRQ0 = timer (PIT). isr_timer is pointed at by the IDT 0x20 gate. */
isr_timer:
    pusha                      /* save EAX..EDI (32 bytes)            */
    call  timer_tick_handler   /* C: count tick + EOI                 */
    popa                       /* restore EAX..EDI                    */
    iret                       /* pop EIP/CS/EFlags -> resumed code   */
