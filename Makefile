# LINDOWS Makefile — 32-bit kernel via Multiboot.
#
# Toolchain (on this Windows host): a prebuilt i686-elf cross toolchain.
#   CC=i686-elf-gcc  AS=i686-elf-as  LD=i686-elf-ld
#
# NOTE: `make` is not installed on this host; the canonical build is
#   & "C:\Program Files\Git\bin\bash.exe" tools/build_direct.sh [PROFILE]
# This file mirrors the script so both stay in sync.
#
# Usage:
#   make                  # build kernel/lindows.bin
#   make PROFILE=developer
#   make smoke            # build + boot in QEMU + assert serial output

PROFILE  ?= minimal

CC       ?= i686-elf-gcc
AS       ?= i686-elf-as
LD       ?= i686-elf-ld
CFLAGS   = -ffreestanding -O2 -Wall -Wextra -nostdlib -fno-asynchronous-unwind-tables -fno-stack-protector -Ikernel
ASFLAGS  = --32 -Iboot
LDFLAGS  = -T kernel/linker.ld

KERNEL_BIN = kernel/lindows.bin

QEMU      ?= C:/OS/AI-OS/vendor/qemu/qemu-system-i386.exe

K_OBJS = kernel/console.o kernel/panic.o kernel/gdt.o kernel/idt.o \
         kernel/pmm.o kernel/vmm.o kernel/heap.o kernel/main.o
A_OBJS = boot/boot.o boot/isr.o

.PHONY: all profile-gen smoke clean
all: $(KERNEL_BIN)

# Generate kernel/gen/profile_gen.h from the selected profile.
profile-gen:
	@mkdir -p kernel/gen
	@printf '#define PROFILE_NAME "%s"\n' "$(PROFILE)" > kernel/gen/profile_gen.h
	@echo "wrote kernel/gen/profile_gen.h ($(PROFILE))"

boot/boot.o: boot/boot.S boot/multiboot.inc
	$(AS) $(ASFLAGS) $< -o $@

boot/isr.o: boot/isr.S
	$(AS) $(ASFLAGS) $< -o $@

kernel/%.o: kernel/%.c kernel/gen/profile_gen.h
	$(CC) $(CFLAGS) -Ikernel/gen -c $< -o $@

$(KERNEL_BIN): profile-gen $(A_OBJS) $(K_OBJS)
	$(LD) $(LDFLAGS) $(A_OBJS) $(K_OBJS) -o $@

smoke: $(KERNEL_BIN)
	tools/smoke_test.sh

clean:
	rm -rf build kernel/gen kernel/*.o kernel/*.bin boot/*.o ser_smoke.log
