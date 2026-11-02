#!/bin/bash
# QEMU smoke test. Boots the kernel, types through the keyboard, drives the
# shell over UART, and fails on missing markers, a bad scanout, or panic.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CODE="$ROOT/firmware/AAVMF_CODE.fd"
VARS="$ROOT/firmware/AAVMF_VARS.fd"
ESP="$ROOT/esp.img"

RESOLUTION="${RESOLUTION:-1280x800}"
if [[ ! "$RESOLUTION" =~ ^([0-9]+)x([0-9]+)$ ]]; then
  echo "qemu test: invalid RESOLUTION '$RESOLUTION' (expected WxH)"
  exit 1
fi
DISPLAY_WIDTH="${BASH_REMATCH[1]}"
DISPLAY_HEIGHT="${BASH_REMATCH[2]}"
if [ "$DISPLAY_WIDTH" -gt 1920 ] || [ "$DISPLAY_HEIGHT" -gt 1200 ]; then
  echo "qemu test: RESOLUTION $RESOLUTION exceeds the kernel limit of 1920x1200"
  exit 1
fi

ACPI="${ACPI:-off}"
case "$ACPI" in
  on | off) ;;
  *)
    echo "qemu test: ACPI must be on or off"
    exit 1
    ;;
esac

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
  -M virt,gic-version=3,acpi=$ACPI \
  -cpu cortex-a72 \
  -smp 2 \
  -m 512M \
  -nic none \
  -drive if=pflash,format=raw,file="$CODE",readonly=on \
  -drive if=pflash,format=raw,file="$VARS_TMP" \
  -drive file="$ESP",format=raw,if=virtio \
  -device virtio-gpu-pci,xres=$DISPLAY_WIDTH,yres=$DISPLAY_HEIGHT \
  -device virtio-keyboard-pci \
  -serial "unix:$SOCKET,server=on,wait=off" \
  -display none \
  -monitor "unix:$MONITOR,server=on,wait=off" \
  2>/dev/null &
QEMU_PID=$!

CLIENT_STATUS=0
python3 "$ROOT/scripts/uart_test_client.py" "$SOCKET" "$LOG" 120 "$SERIAL_DIR" &
CLIENT_PID=$!

# The serial client pauses after the clear command so the scanout can be
# checked while the display is empty.
CLEAR_STATUS=0
CLEAR_READY=0
for _ in $(seq 1 480); do
  if [ -f "$SERIAL_DIR/clear-ready" ]; then
    CLEAR_READY=1
    break
  fi
  sleep 0.25
done
if [ "$CLEAR_READY" -eq 1 ]; then
  sleep 1
  python3 "$ROOT/scripts/check-screen.py" "$MONITOR" \
    "$SERIAL_DIR/clear.ppm" --cleared || CLEAR_STATUS=$?
else
  echo "qemu test: clear marker missing"
  CLEAR_STATUS=1
fi
touch "$SERIAL_DIR/clear-continue"
wait "$CLIENT_PID" || CLIENT_STATUS=$?

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

check_absent() {
  if grep -q "$1" "$LOG" 2>/dev/null; then
    echo "qemu test: FAIL unexpected '$1'"
    FAILED=1
  fi
}

check "fs: fat32 file read ok"
check "virtio-gpu: scanout ok"
check "virtio-input: ready"
check "hardening: wx attributes ok"
check "smp: cpus online: 2"
check "smp: ipi count: 1"
check "smp: worker cpus: 3"
check "mem: address space switch ok"
check "sched: round robin ok"
check "user: spawn pid"
check "gemabox> "
check "commands: help, echo, cat, ls, clear, acpi, uptime, sleep, ps, dmesg, loglevel, persist, mkdir, rm, mv, halt, shutdown, reboot"
check "uptime: "
check "pid ppid"
check "/system/boot/init"
check "dmesg:"
check "loglevel: info (2)"
check "loglevel: debug (3)"
check "e/"
check_absent "mkdir: failed"
check_absent "mv: failed"
check_absent "rm: failed"
check_absent "sleep: missing"
check_absent "sleep: invalid"
check_absent "sleep: failed"
check "system/"
check "ls: list failed"
check "run: spawn failed"
check "hello from applications"
check "append ok"
check "process: remaining: 0"
check "kernel: halt"

if [ "$ACPI" = "on" ]; then
  check "platform: acpi parsed"
  check "acpi: tables"
  check "ACPI revision"
  check "MADT cpus"
  check "GICD"
  check "SPCR uart"
  check "MCFG ecam"
  check "FADT flags"
  check "ARM boot"
  check "PSCI 1"
  check "reset reg"
else
  check "ACPI unavailable"
fi

count_check() {
  local found
  found="$(grep -c "$1" "$LOG" 2>/dev/null || true)"
  if [ "${found:-0}" -lt "$2" ]; then
    echo "qemu test: FAIL expected $2 of '$1', got ${found:-0}"
    FAILED=1
  fi
}

count_check "hello from el0" 2
count_check "init ok" 1
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

if [ "$CLEAR_STATUS" -ne 0 ]; then
  echo "qemu test: FAIL framebuffer clear"
  FAILED=1
fi

if grep -q "panic:" "$LOG" 2>/dev/null; then
  echo "qemu test: FAIL panic detected"
  FAILED=1
fi

if [ "$FAILED" -ne 0 ] || [ "$CLIENT_STATUS" -ne 0 ]; then
  cp "$LOG" "$ROOT/build/qemu-serial.log" 2>/dev/null || true
fi

if [ "$FAILED" -ne 0 ]; then
  echo "--- serial tail ---"
  tail -n 40 "$LOG" || true
  exit 1
fi

echo "qemu test: OK"
