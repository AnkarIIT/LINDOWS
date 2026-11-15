#!/bin/bash
# LINDOWS Phase 3 — 64-bit kernel build script
# Builds the x86_64 ELF64 kernel and creates the FAT32 disk image

set -e

# Toolchain (gitignored, downloaded separately)
TBIN="tools/x86_64-elf/bin"

echo "=== LINDOWS 64-bit kernel build ==="

# 1. Assemble boot/boot64.S
echo "== Assembling boot/boot64.S ==="
"$TBIN/x86_64-elf-as.exe" -o boot/boot64.o boot/boot64.S

# 2. Compile kernel/main64.c
echo "== Compiling kernel/main64.c ==="
"$TBIN/x86_64-elf-gcc.exe" -c -o kernel/main64.o kernel/main64.c \
    -ffreestanding -mno-red-zone -mcmodel=kernel -Wall

# 3. Compile supporting library files needed by main64.c
echo "== Compiling console/panic/gdt/idt/pmm/sched ==="
"$TBIN/x86_64-elf-gcc.exe" -c -o kernel/console.o kernel/console.c \
    -ffreestanding -mno-red-zone -mcmodel=kernel -Wall
"$TBIN/x86_64-elf-gcc.exe" -c -o kernel/panic.o kernel/panic.c \
    -ffreestanding -mno-red-zone -mcmodel=kernel -Wall
"$TBIN/x86_64-elf-gcc.exe" -c -o kernel/gdt.o kernel/gdt.c \
    -ffreestanding -mno-red-zone -mcmodel=kernel -Wall
"$TBIN/x86_64-elf-gcc.exe" -c -o kernel/idt.o kernel/idt.c \
    -ffreestanding -mno-red-zone -mcmodel=kernel -Wall
# Skip pmm/vmm/heap for milestone-1 — Limine loader handles memory

# 4. Link the ELF64 kernel
echo "== Linking kernel ELF64 ==="
"$TBIN/x86_64-elf-ld.exe" -T kernel/linker64.ld \
    boot/boot64.o \
    kernel/main64.o \
    kernel/console.o \
    kernel/panic.o \
    kernel/gdt.o \
    kernel/idt.o \
    -o kernel/lindows64.bin

# 5. Verify ELF64
echo "== Verifying ELF64 ==="
file kernel/lindows64.bin
"$TBIN/x86_64-elf-objdump.exe" -f kernel/lindows64.bin > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "  ELF64 verification passed"
else
    echo "  ELF64 verification FAILED"
    exit 1
fi

# 6. Run mkimg.py to create the FAT32 disk image
echo "== Creating FAT32 disk image ==="
python tools/mkimg.py

# 7. Run Limine BIOS install
echo "== Installing Limine bootloader ==="
LIMINE_TOOL="tools/limine/bin/limine-binary/limine-tool-windows-x86/limine.exe"
if [ -f "$LIMINE_TOOL" ]; then
    WIN_IMG=$(cygpath -w boot/lindows.img)
    "$LIMINE_TOOL" bios-install "$WIN_IMG"
else
    echo "  Limine tool not found at: $LIMINE_TOOL"
    echo "  Skipping limine bios-install"
fi

echo "=== Build complete ==="
echo "Kernel: kernel/lindows64.bin"
echo "Image: boot/lindows.img"
echo "Boot command: qemu-system-i386 -drive file=boot/lindows.img -serial file:ser64.log -display none -cpu max"