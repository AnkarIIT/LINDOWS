#!/usr/bin/env bash
# smoke_test.sh — the automated boot test.
# Builds the kernel, boots it headlessly in QEMU for a few seconds, then
# asserts on the captured serial log. Exit 0 = boot verified.
#
# Env overrides:
#   LINDOWS_QEMU  path to qemu-system-i386 (default: the vendored 9.2.0 build)
set -uo pipefail

ROOT="${LINDOWS_ROOT:-C:/OS/LINDOWS}"
QEMU="${LINDOWS_QEMU:-C:/OS/AI-OS/vendor/qemu/qemu-system-i386.exe}"
LOG="ser_smoke.log"
BOOT_SECONDS="${LINDOWS_BOOT_SECONDS:-8}"

cd "$ROOT" || { echo "FATAL: cannot cd $ROOT"; exit 2; }

echo ">> building"
./tools/build_direct.sh >/dev/null 2>&1 || { echo "FAIL: build failed (run tools/build_direct.sh for details)"; exit 1; }

[ -x "$QEMU" ] || { echo "FAIL: qemu not found at $QEMU (set LINDOWS_QEMU)"; exit 2; }

rm -f "$LOG"
echo ">> booting in QEMU (headless, ${BOOT_SECONDS}s)"
"$QEMU" -kernel kernel/lindows.bin -serial "file:$LOG" -display none &
QPID=$!
sleep "$BOOT_SECONDS"
kill "$QPID" 2>/dev/null

echo ">> serial log:"
cat "$LOG"

STATUS=0
grep -q "LINDOWS boot"          "$LOG" || { echo "FAIL: banner missing";       STATUS=1; }
grep -q "timer verified"        "$LOG" || { echo "FAIL: timer not verified";   STATUS=1; }
grep -q "boot ok"               "$LOG" || { echo "FAIL: boot-ok line missing"; STATUS=1; }
grep -q "heap verified"         "$LOG" || { echo "FAIL: heap not verified";    STATUS=1; }
grep -q "sched verified"        "$LOG" || { echo "FAIL: scheduler not verified"; STATUS=1; }
grep -q "<A><B><A><B><A><B>"    "$LOG" || { echo "FAIL: no A/B alternation (not round-robin)"; STATUS=1; }
A_TURNS=$(grep -o "<A>" "$LOG" | wc -l)
B_TURNS=$(grep -o "<B>" "$LOG" | wc -l)
[ "$A_TURNS" -ge 5 ] || { echo "FAIL: task A ran $A_TURNS turns (< 5)"; STATUS=1; }
[ "$B_TURNS" -ge 5 ] || { echo "FAIL: task B ran $B_TURNS turns (< 5)"; STATUS=1; }
grep -q "PANIC"                 "$LOG" && { echo "FAIL: kernel panicked";      STATUS=1; }

# A reboot loop (e.g. triple fault) reprints the banner — QEMU+SeaBIOS reset
# the machine and boot again. Exactly ONE banner is healthy.
if [ "$(grep -c 'LINDOWS boot' "$LOG")" -gt 1 ]; then
    echo "FAIL: boot repeated => reset loop (triple fault?)"; STATUS=1
fi

[ "$STATUS" -eq 0 ] && echo ">> SMOKE TEST PASSED" || echo ">> SMOKE TEST FAILED"
exit "$STATUS"
