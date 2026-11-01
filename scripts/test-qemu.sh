#!/bin/bash
# QEMU smoke test. Boots the kernel, waits for known markers, injects four
# bytes into the UART to exercise RX, and fails on missing markers or panic.

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

SERIAL_DIR="$(mktemp -d)"
SOCKET="$SERIAL_DIR/serial.sock"
LOG="$SERIAL_DIR/serial.log"
VARS_TMP="$(mktemp /tmp/AAVMF_VARS.XXXXXX.fd)"
cleanup() {
  if [ -n "${QEMU_PID:-}" ]; then
    kill "$QEMU_PID" 2>/dev/null || true
  fi
  rm -f "$VARS_TMP"
  rm -rf "$SERIAL_DIR"
}
trap cleanup EXIT

cp "$VARS" "$VARS_TMP"

qemu-system-aarch64 \
  -M virt,gic-version=3,acpi=off \
  -cpu cortex-a72 \
  -smp 2 \
  -m 512M \
  -nic none \
  -drive if=pflash,format=raw,file="$CODE",readonly=on \
  -drive if=pflash,format=raw,file="$VARS_TMP" \
  -drive file="$ESP",format=raw,if=virtio \
  -serial "unix:$SOCKET,server=on,wait=off" \
  -display none \
  -monitor none \
  2>/dev/null &
QEMU_PID=$!

CLIENT_STATUS=0
python3 "$ROOT/scripts/uart_test_client.py" "$SOCKET" "$LOG" 120 || CLIENT_STATUS=$?

kill "$QEMU_PID" 2>/dev/null || true
wait "$QEMU_PID" 2>/dev/null || true
QEMU_PID=""

if [ "$CLIENT_STATUS" -ne 0 ]; then
  echo "qemu test: serial client failed"
fi

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
check "uart: rx ok"
check "hello from el0"
check "mmap ok"
check "user: exit ok"
check "kernel: halt"

if grep -q "panic:" "$LOG" 2>/dev/null; then
  echo "qemu test: FAIL panic detected"
  FAILED=1
fi

if [ "$FAILED" -ne 0 ]; then
  echo "--- serial tail ---"
  tail -n 40 "$LOG" || true
  exit 1
fi

echo "qemu test: OK"
