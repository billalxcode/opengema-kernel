#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CODE="$ROOT/firmware/AAVMF_CODE.fd"
VARS="$ROOT/firmware/AAVMF_VARS.fd"
ESP="$ROOT/esp"

usage() {
  echo "usage: $0 [--gui=on|off]"
  echo "  --gui=off  headless, serial only (default)"
  echo "  --gui=on   window QEMU dengan virtio-gpu, serial tetap di terminal"
  echo "  --gui      sama dengan --gui=on"
  echo "  --no-gui   sama dengan --gui=off"
}

GUI=0
for arg in "$@"; do
  case "$arg" in
    --gui | --gui=on | --gui=yes | --gui=1)
      GUI=1
      ;;
    --gui=off | --gui=no | --gui=0 | --no-gui)
      GUI=0
      ;;
    -h | --help)
      usage
      exit 0
      ;;
    *)
      echo "unknown arg: $arg"
      usage
      exit 1
      ;;
  esac
done

if [ ! -f "$CODE" ] || [ ! -f "$VARS" ]; then
  echo "missing firmware in $ROOT/firmware"
  echo "run: ./scripts/get-firmware.sh"
  exit 1
fi

if [ ! -f "$ESP/EFI/BOOT/BOOTAA64.EFI" ]; then
  echo "missing ESP image: $ESP/EFI/BOOT/BOOTAA64.EFI"
  echo "run: ./scripts/build.sh"
  exit 1
fi

# Prefer GPT esp.img (reliable EDK2 boot). Fallback to QEMU virtual FAT.
IMG="$ROOT/esp.img"
if [ -f "$IMG" ]; then
  DISK_ARGS="-drive file=$IMG,format=raw,if=virtio"
else
  echo "warn: esp.img missing, fallback to fat:rw (may not boot)"
  echo "run: ./scripts/make-esp.sh"
  DISK_ARGS="-drive file=fat:rw:$ESP,format=raw,media=disk"
fi

# GUI needs a display adapter and input devices. macOS uses cocoa.
if [ "$GUI" -eq 1 ]; then
  DISPLAY_ARGS="-display cocoa -device virtio-gpu-pci -device virtio-keyboard-pci -device virtio-mouse-pci"
else
  DISPLAY_ARGS="-display none"
fi

# Copy VARS to a writable temp file so boot does not dirty the repo file.
VARS_TMP="$(mktemp /tmp/AAVMF_VARS.XXXXXX.fd)"
cp "$VARS" "$VARS_TMP"
trap 'rm -f "$VARS_TMP"' EXIT

qemu-system-aarch64 \
  -M virt,gic-version=3 \
  -cpu cortex-a72 \
  -smp 2 \
  -m 512M \
  -nic none \
  -drive if=pflash,format=raw,file="$CODE",readonly=on \
  -drive if=pflash,format=raw,file="$VARS_TMP" \
  $DISK_ARGS \
  $DISPLAY_ARGS \
  -serial mon:stdio \
  -d guest_errors
