/*
 * LINDOWS Phase 2 — interrupt + exception stubs (GNU as, .code32).
 *
 * Every gate stub does:
 *   [maybe] push fake error code (so ALL frames look identical)
 *   pusha                     (8 GPRs)
 *   push $<vector>            (so C knows which gate fired)
 *   jmp  isr_common_noerr | isr_common_err
 *
 * The common stub calls C `isr_dispatch(vector, regs)` and then unwinds and
 * irets. `regs` points at the pushed GPR block, so C can dump registers in a
 * panic. Uniform frame layout (dwords, ascending addresses):
 *
 *     regs[0..7] = EDI ESI EBP ESP(old) EBX EDX ECX EAX
 *     regs[8]    = error code (real, or the fake 0 we pushed)
 *     regs[9]    = EIP at the interrupt (the fault address for exceptions)
 *     regs[10]   = CS
 *     regs[11]   = EFLAGS
 *
 * Error-code vectors (Intel SDM 6.15): 8,10,11,12,13,14,17,21. The CPU pushes
 * the error code itself for those; all others get our fake 0 so `iret` sees the
 * same stack shape in both classes.
 */
.code32

.section .text

/* ---- exception stubs (vectors 0..31) ---- */
.macro ISR_NOERR n
    .global isr\n
isr\n:
    push $0               /* fake error code -> uniform frame          */
    pusha
    push $\n              /* vector                                    */
    jmp isr_common_noerr
.endm

.macro ISR_ERR n
    .global isr\n
isr\n:
    pusha                 /* error code already pushed by the CPU      */
    push $\n
    jmp isr_common_err
.endm

/* ---- common tails ---- */
.section .text
isr_common_noerr:
    lea 4(%esp), %eax     /* regs = &EDI (top of pusha block)          */
    push %eax             /* arg2: regs                                */
    push 4(%esp)          /* arg1: vector (shifted by the push above)  */
    call isr_dispatch
    add $8, %esp          /* pop the two dispatch args                 */
    add $4, %esp          /* drop the pushed vector                    */
    popa
    add $4, %esp          /* pop fake error code                       */
    iret

isr_common_err:
    lea 4(%esp), %eax
    push %eax
    push 4(%esp)
    call isr_dispatch
    add $8, %esp
    add $4, %esp          /* drop the pushed vector                    */
    popa
    add $4, %esp          /* pop the CPU-pushed error code             */
    iret

/* ---- 32 exceptions: 0..7,9,15,16,18..20,22..31 no-err; 8,10..14,17,21 err ---- */
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

/* ---- 16 legacy-PIC IRQ stubs (vectors 32..47, never carry error codes) ---- */
ISR_NOERR 32
ISR_NOERR 33
ISR_NOERR 34
ISR_NOERR 35
ISR_NOERR 36
ISR_NOERR 37
ISR_NOERR 38
ISR_NOERR 39
ISR_NOERR 40
ISR_NOERR 41
ISR_NOERR 42
ISR_NOERR 43
ISR_NOERR 44
ISR_NOERR 45
ISR_NOERR 46
ISR_NOERR 47
