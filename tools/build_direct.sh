#!/usr/bin/env bash
# build_direct.sh — canonical LINDOWS build (GNU make is not installed on this
# host; invoke with Git Bash:  & "C:\Program Files\Git\bin\bash.exe" tools/build_direct.sh)
#
# Assumes: tools/i686-elf/ already extracted from tools/i686-elf-tools-windows.zip
# Usage:   tools/build_direct.sh [PROFILE]        (PROFILE currently names only;
#          the YAML manifests are not yet parsed — see profiles/README.md)
set -euo pipefail

ROOT="${LINDOWS_ROOT:-C:/OS/LINDOWS}"
TBIN="$ROOT/tools/i686-elf/bin"
cd "$ROOT"

PROFILE="${1:-minimal}"
echo ">> profile: $PROFILE"

# 1. generate kernel/gen/profile_gen.h (mirrors the Makefile 'profile-gen' target)
mkdir -p kernel/gen
printf '#define PROFILE_NAME "%s"\n' "$PROFILE" > kernel/gen/profile_gen.h
echo "   profile_gen.h: $(cat kernel/gen/profile_gen.h)"

# 2. assemble boot + interrupt stubs (gas: --32, include dir = boot/)
"$TBIN/i686-elf-as.exe"  --32 -Iboot -o boot/boot.o boot/boot.S
"$TBIN/i686-elf-as.exe"  --32 -Iboot -o boot/isr.o  boot/isr.S
echo ">> boot.o + isr.o assembled"

# 3. compile the kernel (freestanding, no host libc)
CFLAGS=(-ffreestanding -O2 -Wall -Wextra -nostdlib
        -fno-asynchronous-unwind-tables -fno-stack-protector
        -Ikernel -Ikernel/gen)
for src in console panic gdt idt main; do
    "$TBIN/i686-elf-gcc.exe" "${CFLAGS[@]}" -c "kernel/$src.c" -o "kernel/$src.o"
done
echo ">> console.o panic.o gdt.o idt.o main.o compiled"

# 4. link (direct ld; minimal kernel needs no libgcc)
"$TBIN/i686-elf-ld.exe" -T kernel/linker.ld boot/boot.o boot/isr.o \
    kernel/console.o kernel/panic.o kernel/gdt.o kernel/idt.o kernel/main.o \
    -o kernel/lindows.bin
echo ">> linked: kernel/lindows.bin ($(stat -c %s kernel/lindows.bin) bytes)"

# 5. verify the resulting Multiboot ELF
"$TBIN/i686-elf-readelf.exe" -h kernel/lindows.bin | grep -E "Class:|Machine:|Entry"
echo ">> multiboot header check:"
python - <<'PY'
import struct
b = open('kernel/lindows.bin', 'rb').read()
i = b.find(b'\x02\xb0\xad\x1b')
assert i >= 0, "multiboot magic not found"
magic, flags, checksum = struct.unpack_from('<III', b, i)
print(f"   @ 0x{i:x}: magic={hex(magic)} flags={hex(flags)} checksum={hex(checksum)}")
assert (magic + flags + checksum) % (1 << 32) == 0, "bad checksum"
print("   checksum valid (sum mod 2^32 == 0)  OK")
PY
echo ">> DONE: LINDOWS kernel built + verified as a Multiboot ELF"
