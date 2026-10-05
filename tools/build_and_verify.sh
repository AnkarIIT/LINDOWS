#!/usr/bin/env bash
# build_and_verify.sh — extract the prebuilt i686-elf toolchain (if needed),
# compile the Phase-1 LINDOWS kernel, and verify the resulting Multiboot ELF.
#
# Run from the project root:  ./tools/build_and_verify.sh [PROFILE=minimal]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
PROFILE="${1:=minimal}"
export PROFILE

ZIP="tools/i686-elf-tools-windows.zip"
DEST="tools/i686-elf"

# --- 1. Ensure the cross-toolchain is extracted + on PATH ---------------------
if [ ! -x "$DEST/bin/i686-elf-gcc.exe" ] && [ ! -x "$DEST/bin/i686-elf-gcc" ]; then
  echo ">> extracting cross-toolchain (one time, ~900MB zip) ..."
  [ -f "$ZIP" ] || { echo "!! toolchain zip missing: $ZIP"; exit 1; }
  python - "$ZIP" "$DEST" <<'PY'
import sys, zipfile, os
z, dest = sys.argv[1], sys.argv[2]
os.makedirs(dest, exist_ok=True)
with zipfile.ZipFile(z) as zf:
    zf.extractall(dest)
print("extracted", len(zf.namelist()), "entries ->", dest)
PY
fi

# Locate the bin dir (lordmilko zips put bin/ at the archive root, but be safe).
BINDIR="$DEST/bin"
if [ ! -x "$BINDIR/i686-elf-gcc" ] && [ ! -x "$BINDIR/i686-elf-gcc.exe" ]; then
  BINDIR="$(python - "$DEST" <<'PY'
import sys, os, glob
d=sys.argv[1]
for root,_,files in os.walk(d):
    if "i686-elf-gcc.exe" in files or "i686-elf-gcc" in files:
        print(root); break
PY
)"
  [ -n "$BINDIR" ] || { echo "!! i686-elf-gcc not found under $DEST"; exit 1; }
fi
export PATH="$BINDIR:$PATH"
echo ">> toolchain: $(i686-elf-gcc -dumpversion) in $BINDIR"

# --- 2. Build -----------------------------------------------------------------
echo ">> building (PROFILE=$PROFILE) ..."
make clean >/dev/null 2>&1 || true
make

BIN="kernel/lindows.bin"
[ -f "$BIN" ] || { echo "!! kernel ELF not produced"; exit 1; }

# --- 3. Verify the ELF ---------------------------------------------------------
echo ">> ELF header:"
i686-elf-readelf -h "$BIN" | sed -n '1,20p'
echo ">> sections:"
i686-elf-objdump -h "$BIN" | sed -n '1,20p'

# Multiboot v1 magic = 0x1BADB002, stored little-endian as 02 ad 1b 00.
echo ">> multiboot header present in binary:"
python - "$BIN" <<'PY'
import sys
b=open(sys.argv[1],"rb").read()
i=b.find(b"\x02\xad\x1b\x00")
print("  multiboot magic @ offset", hex(i) if i>=0 else "NOT FOUND")
PY

echo "OK"
