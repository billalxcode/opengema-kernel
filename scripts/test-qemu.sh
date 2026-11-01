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
MONITOR="$SERIAL_DIR/monitor.sock"
SCREEN="$SERIAL_DIR/screen.ppm"
VARS_TMP="$(mktemp /tmp/AAVMF_VARS.XXXXXX)"
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
  -device virtio-gpu-pci \
  -serial "unix:$SOCKET,server=on,wait=off" \
  -display none \
  -monitor "unix:$MONITOR,server=on,wait=off" \
  2>/dev/null &
QEMU_PID=$!

CLIENT_STATUS=0
python3 "$ROOT/scripts/uart_test_client.py" "$SOCKET" "$LOG" 120 || CLIENT_STATUS=$?

# Give the last framebuffer flush time to land, then capture the scanout.
sleep 1
SCREEN_STATUS=0
python3 "$ROOT/scripts/check-screen.py" "$MONITOR" "$SCREEN" || SCREEN_STATUS=$?

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
check "virtio-gpu: scanout ok"
check "hardening: wx attributes ok"
check "smp: cpus online: 2"
check "smp: ipi count: 1"
check "smp: worker cpus: 3"
check "mem: address space switch ok"
check "sched: round robin ok"
check "user: spawn pid"
check "gemabox> "
check "commands: help, echo, cat, exit"
check "process: remaining: 0"
check "kernel: halt"

count_check() {
  local found
  found="$(grep -c "$1" "$LOG" 2>/dev/null || true)"
  if [ "${found:-0}" -lt "$2" ]; then
    echo "qemu test: FAIL expected $2 of '$1', got ${found:-0}"
    FAILED=1
  fi
}

count_check "hello from el0" 2
count_check "file ok" 2
count_check "mmap ok" 2
count_check "child ok" 2
count_check "spawn ok" 2
count_check "write ok" 1
count_check "user: exit status: 0" 4

if [ "$SCREEN_STATUS" -ne 0 ]; then
  echo "qemu test: FAIL framebuffer screen"
  FAILED=1
fi

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
