#!/usr/bin/env bash
# build_direct.sh — build + verify the Phase-1 LINDOWS kernel with the prebuilt
# i686-elf cross-toolchain DIRECTLY (GNU make is not installed on this host).
#
# Assumes: tools/i686-elf/ already extracted from tools/i686-elf-tools-windows.zip
# Usage:   ./tools/build_direct.sh [PROFILE=minimal]
set -euo pipefail

# Native Windows path (forward slashes). Host-specific.
ROOT="C:/OS/LINDOWS"
TBIN="$ROOT/tools/i686-elf/bin"
cd "$ROOT"

PROFILE="${1:-minimal}"
echo ">> profile: $PROFILE"

# 1. generate kernel/gen/profile_gen.h (mirrors the Makefile 'profile-gen' target)
mkdir -p kernel/gen
printf '#define PROFILE_NAME "%s"\n' "$PROFILE" > kernel/gen/profile_gen.h
echo -n "   profile_gen.h: "; cat kernel/gen/profile_gen.h

# 2. assemble the boot stub (gas: --32, include dir = boot/)
"$TBIN/i686-elf-as.exe"  --32 -Iboot -o boot/boot.o boot/boot.S
echo ">> boot.o assembled"

# 3. compile the kernel (freestanding, no host libc)
"$TBIN/i686-elf-gcc.exe" -ffreestanding -O2 -Wall -Wextra -nostdlib -Ikernel/gen -c kernel/main.c -o kernel/main.o
echo ">> main.o compiled"

# 4. link (direct ld; minimal kernel needs no libgcc)
"$TBIN/i686-elf-ld.exe" -T kernel/linker.ld boot/boot.o kernel/main.o -o kernel/lindows.bin
echo ">> linked: kernel/lindows.bin"
ls -la kernel/lindows.bin

# 5. verify the resulting Multiboot ELF
echo ">> ELF header"; "$TBIN/i686-elf-readelf.exe" -h kernel/lindows.bin | sed -n '1,16p'
echo ">> sections";   "$TBIN/i686-elf-objdump.exe" -h kernel/lindows.bin | sed -n '1,12p'
echo ">> multiboot header check:"
python - <<'PY'
import struct
b=open('kernel/lindows.bin','rb').read()
i=b.find(b'\x02\xb0\xad\x1b')
assert i>=0, "multiboot magic not found"
magic,flags,checksum=struct.unpack_from('<III',b,i)
print(f"   @ 0x{i:x}: magic={hex(magic)} flags={hex(flags)} checksum={hex(checksum)}")
assert (magic+flags+checksum)%(1<<32)==0, "bad checksum"
print("   checksum valid (sum mod 2^32 == 0)  OK")
PY
echo ">> DONE: LINDOWS kernel built + verified as a Multiboot ELF"
