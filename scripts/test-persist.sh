#!/bin/bash
# Writes and reads /volumes/boot/persist.bin across two QEMU boots.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CODE="$ROOT/firmware/AAVMF_CODE.fd"
VARS="$ROOT/firmware/AAVMF_VARS.fd"
ESP="$ROOT/esp.img"

if [ ! -f "$CODE" ] || [ ! -f "$VARS" ] || [ ! -f "$ESP" ]; then
  echo "persist probe: firmware or esp image missing"
  exit 1
fi

TMP_DIR="$(mktemp -d)"
DISK="$TMP_DIR/disk.img"
VARS_TMP="$TMP_DIR/vars.fd"
SOCKET="$TMP_DIR/serial.sock"
LOG="$TMP_DIR/serial.log"
cleanup() {
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT

cp "$ESP" "$DISK"

run_boot() {
  local command="$1"
  local marker="$2"
  rm -f "$SOCKET" "$LOG"
  cp "$VARS" "$VARS_TMP"
  qemu-system-aarch64 \
    -M virt,gic-version=3,acpi=on \
    -cpu cortex-a72 \
    -smp 2 \
    -m 512M \
    -nic none \
    -drive if=pflash,format=raw,file="$CODE",readonly=on \
    -drive if=pflash,format=raw,file="$VARS_TMP" \
    -drive file="$DISK",format=raw,if=virtio \
    -device virtio-gpu-pci \
    -device virtio-keyboard-pci \
    -serial "unix:$SOCKET,server=on,wait=off" \
    -display none \
    -monitor none \
    2>/dev/null &
  local qemu_pid=$!
  local status=0
  python3 "$ROOT/scripts/persist_client.py" "$SOCKET" "$LOG" \
    "$command" "$marker" 90 || status=$?
  kill "$qemu_pid" 2>/dev/null || true
  wait "$qemu_pid" 2>/dev/null || true
  if [ "$status" -ne 0 ]; then
    echo "persist probe: '$command' failed"
    tail -n 20 "$LOG" || true
    return 1
  fi
  return 0
}

run_boot $'persist write\npersist create' "persist: create ok"
run_boot $'persist read\npersist read2' "persist: read2 ok"
run_boot $'persist mkdir\npersist delete' "persist: delete ok"
run_boot $'persist rename\nls /volumes/boot' "RENAMED.BIN"
run_boot "persist truncate" "persist: truncate ok"
run_boot "persist empty" "persist: empty ok"
run_boot "persist long" "persist: long ok"
run_boot "persist readlong" "persist: readlong ok"
run_boot $'mv "/volumes/boot/persist long name.txt" /volumes/boot/long2.txt\ncat /volumes/boot/long2.txt' "persist ok"
run_boot $'rm /volumes/boot/long2.txt\ncat /volumes/boot/long2.txt' "cat: open failed"
echo "persist probe: OK"
