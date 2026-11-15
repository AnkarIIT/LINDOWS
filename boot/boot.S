/*
 * LINDOWS boot stub — GNU as syntax.
 *
 * Runs at PHYSICAL 1 MiB with paging off (linear == physical). The kernel
 * proper is linked at 0xC0100000 (VMA) but loaded at 0x100000 (LMA), so
 * phys = vma - V2P inside the kernel image. This stub:
 *
 *   1. validates the multiboot magic (EAX == 0x2BADB002),
 *   2. builds the early page tables (reserved in .bss by linker.ld, touched
 *      here by physical address):
 *         PD[0]   = early_low_pt  -> identity map 0..4 MiB
 *         PD[768] = early_low_pt  -> the SAME frames aliased at
 *                                    0xC0000000..0xC0400000, so the
 *                                    kernel's higher-half VMAs work
 *         PD[1023]= early_pd      -> recursive (VMM maintains tables via it)
 *   3. enables paging (CR0.PG | WP),
 *   4. keeps executing here (the stub is identity-mapped) and calls kmain
 *      at its higher-half VMA with the kernel (VMA) stack.
 *
 * The multiboot info pointer (EBX) is a PHYSICAL address; low memory stays
 * identity-mapped, so pmm/vmm may keep using it directly.
 */
.code32

.section .multiboot, "ax", @progbits
.include "multiboot.inc"          /* constants — found via -Iboot in ASFLAGS */

/* Multiboot v1 header: must live within the first 8 KiB of the loaded image.
 * First thing in .stub at LMA 0x100000. */
multiboot_header:
    .long MULTIBOOT_HEADER_MAGIC
    .long MULTIBOOT_HEADER_FLAGS
    .long MULTIBOOT_HEADER_CHECKSUM

.section .stub, "ax", @progbits
.global _start
.extern kmain

.set V2P, 0xC0000000

_start:
    cmp $0x2BADB002, %eax         /* did a multiboot loader actually start us? */
    jne bad_magic
    cli                           /* no interrupts until the IDT is up */

    /* ---- early_low_pt[i] = (i * 4 KiB) | P|RW  (identity 0..4 MiB) ---- */
    mov $(early_low_pt - V2P), %edi
    xor %ecx, %ecx
1:  mov %ecx, %eax
    shl $12, %eax                 /* frame phys = i * 4096 */
    or  $0x003, %eax              /* present | writable */
    mov %eax, (%edi)
    add $4, %edi
    inc %ecx
    cmp $1024, %ecx
    jb 1b

    /* ---- early_pd: [0] and [768] -> the same PT (identity + kernel alias),
     *      [1023] -> the PD itself (recursive) ---- */
    mov $(early_pd - V2P), %edi
    mov $(early_low_pt - V2P), %eax
    or  $0x003, %eax
    mov %eax, 0(%edi)             /* PD[0]   : 0x00000000.. identity 4 MiB */
    mov %eax, 3072(%edi)          /* PD[768] : 0xC0000000.. kernel window  */
    mov %edi, %eax                /* PD[1023]: recursive                   */
    or  $0x003, %eax
    mov %eax, 4092(%edi)

    /* ---- switch to paging ---- */
    mov %eax, %esi                /* save the multiboot magic across the CR dance */
    mov $(early_pd - V2P), %eax
    mov %eax, %cr3
    mov %cr0, %eax
    or  $0x80010000, %eax         /* PG (bit 31) | WP (bit 16) */
    mov %eax, %cr0
    jmp 1f                        /* flush the prefetch pipeline */
1:
    /* We are now paging: the stub runs via the identity map, the kernel
     * proper is reachable at its VMA. Install the (VMA) kernel stack and
     * hand off to C. Multiboot v1 leaves %esp undefined, so we set our own
     * (stack lives in .bss, mapped through the kernel window). */
    mov $stack_top, %esp
    xor %ebp, %ebp                /* terminate backtrace chain */
    push %ebx                     /* arg2: multiboot info pointer (physical) */
    push %esi                     /* arg1: magic (saved above) */
    call kmain
hang:
    hlt
    jmp hang

bad_magic:
    /* Not loaded by a multiboot loader — blink a red 'X' top-left and stop. */
    mov $0xB8000, %edi
    mov $0x4F58, %ax              /* 'X', red on black */
    stosw
    cli
1:  hlt
    jmp 1b
