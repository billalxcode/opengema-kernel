#!/bin/bash
# QEMU smoke test. Boots the kernel, waits for known markers, and fails if
# any marker is missing or a panic appears.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CODE="$ROOT/firmware/AAVMF_CODE.fd"
VARS="$ROOT/firmware/AAVMF_VARS.fd"
ESP="$ROOT/esp.img"

if [ ! -f "$CODE" ] || [ ! -f "$VARS" ]; then
  echo "missing firmware, run ./scripts/get-firmware.sh"
  exit 1
fi
if [ ! -f "$ESP" ]; then
  echo "missing esp.img, run ./scripts/make-esp.sh"
  exit 1
fi

LOG="$(mktemp)"
VARS_TMP="$(mktemp /tmp/AAVMF_VARS.XXXXXX.fd)"
cleanup() {
  if [ -n "${QEMU_PID:-}" ]; then
    kill "$QEMU_PID" 2>/dev/null || true
  fi
  rm -f "$VARS_TMP"
}
trap cleanup EXIT

cp "$VARS" "$VARS_TMP"

qemu-system-aarch64 \
  -M virt,gic-version=3 \
  -cpu cortex-a72 \
  -smp 2 \
  -m 512M \
  -nic none \
  -drive if=pflash,format=raw,file="$CODE",readonly=on \
  -drive if=pflash,format=raw,file="$VARS_TMP" \
  -drive file="$ESP",format=raw,if=virtio \
  -serial "file:$LOG" \
  -display none \
  -monitor none \
  2>/dev/null &
QEMU_PID=$!

for _ in $(seq 1 120); do
  if grep -q "kernel: halt" "$LOG" 2>/dev/null; then
    break
  fi
  if ! kill -0 "$QEMU_PID" 2>/dev/null; then
    break
  fi
  sleep 1
done

kill "$QEMU_PID" 2>/dev/null || true
wait "$QEMU_PID" 2>/dev/null || true
QEMU_PID=""

FAILED=0
check() {
  if ! grep -q "$1" "$LOG" 2>/dev/null; then
    echo "qemu test: FAIL missing '$1'"
    FAILED=1
  fi
}

check "fs: fat32 file read ok"
check "hardening: wx attributes ok"
check "smp: cpus online: 2"
check "sched: round robin ok"
check "hello from el0"
check "user: exit ok"
check "kernel: halt"

if grep -q "panic:" "$LOG" 2>/dev/null; then
  echo "qemu test: FAIL panic detected"
  FAILED=1
fi

if [ "$FAILED" -ne 0 ]; then
  echo "--- serial tail ---"
  tail -n 40 "$LOG" || true
  rm -f "$LOG"
  exit 1
fi

rm -f "$LOG"
echo "qemu test: OK"
