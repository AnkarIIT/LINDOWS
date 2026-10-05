; LINDOWS boot stub
; Multiboot (v1) header so GRUB can load us in 32-bit protected mode, plus the
; hand-written entry point that calls kmain. This is the phase-1 bootstrap.

BITS 32

%include "multiboot.inc"   ; MULTIBOOT_HEADER / MAGIC constants

multiboot_header:
    ; GRUB looks for this header in the binary it loads.
    dd MULTIBOOT_HEADER_MAGIC
    dd MULTIBOOT_HEADER_FLAGS
    dd MULTIBOOT_HEADER_CHECKSUM

section .text
global _start
extern kmain

_start:
    cli                                 ; no interrupts until IDT is up
    call kmain                          ; hand off to the C kernel
hang:                                   ; kmain shouldn't return; if it does, stop
    hlt
    jmp hang
