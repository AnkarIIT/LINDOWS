/*
 * LINDOWS Phase 3 — minimal 64-bit long-mode entry
 * The Limine loader loads the ELF64 kernel and jumps to _start
 * in long mode with paging already enabled (CR0.PG, CR4.PAE, EFER.LME).
 * This file does ONLY: set up stack + call kmain64.
 * Segment selectors are assumed to be already set by the loader.
 */

        .globl _start

_start:
        /* Set stack pointer — 4 KiB red zone at 0xffffffff8000f000.
         * The loader's PML4 maps this in the higher-half layout. */
        mov      $0xffffffff8000f000, %rsp

        /* Call C's long-mode main — never returns */
        call     kmain64

        /* hlt loop if kmain64 returns */
        hlt
        .align 4