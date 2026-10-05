# LINDOWS Makefile — stage 1 (32-bit kernel via GRUB multiboot).
#
# Toolchain (on this Windows host): a prebuilt i686-elf cross toolchain.
#   CC=i686-elf-gcc  AS=i686-elf-as  LD=i686-elf-ld
# grub-mkrescue builds the ISO; qemu-system-i386 runs it.
#
# Usage:
#   make            # build kernel/lindows.bin
#   make iso        # build an ISO image
#   make run        # build + run in QEMU
#   make PROFILE=developer run

PROFILE  ?= minimal

CC       ?= i686-elf-gcc
AS       ?= i686-elf-as
LD       ?= i686-elf-ld
CFLAGS   = -ffreestanding -O2 -Wall -Wextra -nostdlib
ASFLAGS  = -f elf32
LDFLAGS  = -T kernel/linker.ld -nostdlib -nodefaultlibs -nostartfiles

KERNEL_BIN = kernel/lindows.bin
ISO        = build/lindows.iso

.PHONY: all profile-gen iso run clean
all: $(KERNEL_BIN)

# Generate kernel/gen/profile_gen.h from the selected profile.
profile-gen:
	@mkdir -p kernel/gen
	@printf '#define PROFILE_NAME "%s"\n' "$(PROFILE)" > kernel/gen/profile_gen.h
	@echo "wrote kernel/gen/profile_gen.h ($(PROFILE))"

boot/boot.o: boot/boot.S boot/multiboot.inc
	$(AS) $(ASFLAGS) $< -o $@

kernel/main.o: kernel/main.c kernel/gen/profile_gen.h
	$(CC) $(CFLAGS) -Ikernel/gen -c $< -o $@

$(KERNEL_BIN): profile-gen boot/boot.o kernel/main.o
	$(LD) $(LDFLAGS) boot/boot.o kernel/main.o -o $@

iso: $(ISO)

$(ISO): $(KERNEL_BIN)
	@mkdir -p build/iso_root/boot/grub
	cp $(KERNEL_BIN) build/iso_root/boot/kernel.bin
	@printf 'set timeout=0\nset default=0\nmenuentry "LINDOWS" { multiboot /boot/kernel.bin }\n' > build/iso_root/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) build/iso_root

run: $(KERNEL_BIN)
	qemu-system-i386 -kernel $(KERNEL_BIN) -serial stdio 2>&1 | tail -20 || echo "(qemu not available)"

clean:
	rm -rf build kernel/gen kernel/*.o kernel/*.bin boot/*.o
