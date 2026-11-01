#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CODE="$ROOT/firmware/AAVMF_CODE.fd"
VARS="$ROOT/firmware/AAVMF_VARS.fd"
ESP="$ROOT/esp"

usage() {
  echo "usage: $0 [--gui=on|off] [--resolution=WxH]"
  echo "  --gui=off  headless, serial only (default)"
  echo "  --gui=on   window QEMU dengan virtio-gpu, serial tetap di terminal"
  echo "  --gui      sama dengan --gui=on"
  echo "  --no-gui   sama dengan --gui=off"
  echo "  --resolution=WxH  resolusi scanout, contoh 1920x1080"
  echo "                    batas kernel saat ini 1920x1200 (default QEMU 1280x800)"
  echo "  --acpi=on|off     tabel firmware, default off (memakai DTB)"
}

GUI=0
RESOLUTION=""
ACPI="off"
while [ "$#" -gt 0 ]; do
  case "$1" in
    --gui | --gui=on | --gui=yes | --gui=1)
      GUI=1
      ;;
    --gui=off | --gui=no | --gui=0 | --no-gui)
      GUI=0
      ;;
    --resolution=*)
      RESOLUTION="${1#*=}"
      ;;
    --resolution)
      if [ "$#" -lt 2 ]; then
        echo "missing value for --resolution"
        usage
        exit 1
      fi
      RESOLUTION="$2"
      shift
      ;;
    --acpi=*)
      ACPI="${1#*=}"
      ;;
    --acpi)
      if [ "$#" -lt 2 ]; then
        echo "missing value for --acpi"
        usage
        exit 1
      fi
      ACPI="$2"
      shift
      ;;
    -h | --help)
      usage
      exit 0
      ;;
    *)
      echo "unknown arg: $1"
      usage
      exit 1
      ;;
  esac
  shift
done

GPU_ARGS="-device virtio-gpu-pci"
if [ -n "$RESOLUTION" ]; then
  if [[ ! "$RESOLUTION" =~ ^([0-9]+)x([0-9]+)$ ]]; then
    echo "invalid resolution: $RESOLUTION (expected WxH, for example 1920x1080)"
    exit 1
  fi
  WIDTH="${BASH_REMATCH[1]}"
  HEIGHT="${BASH_REMATCH[2]}"
  if [ "$WIDTH" -eq 0 ] || [ "$HEIGHT" -eq 0 ]; then
    echo "invalid resolution: $RESOLUTION (width and height must be positive)"
    exit 1
  fi
  if [ "$WIDTH" -gt 1920 ] || [ "$HEIGHT" -gt 1200 ]; then
    echo "resolution $RESOLUTION exceeds the kernel limit of 1920x1200"
    exit 1
  fi
  GPU_ARGS="-device virtio-gpu-pci,xres=$WIDTH,yres=$HEIGHT"
fi

case "$ACPI" in
  on | off) ;;
  *)
    echo "invalid ACPI value: $ACPI (expected on or off)"
    exit 1
    ;;
esac

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

# The input driver currently binds one virtio-input device. Attach the
# keyboard so the console path is ready; mouse support will join when the
# driver handles multiple input devices.
INPUT_ARGS="-device virtio-keyboard-pci"
if [ "$GUI" -eq 1 ]; then
  DISPLAY_ARGS="-display cocoa"
else
  DISPLAY_ARGS="-display none"
fi
DISPLAY_ARGS="$DISPLAY_ARGS $GPU_ARGS $INPUT_ARGS"

# Copy VARS to a writable temp file so boot does not dirty the repo file.
VARS_TMP="$(mktemp /tmp/AAVMF_VARS.XXXXXX)"
cp "$VARS" "$VARS_TMP"
trap 'rm -f "$VARS_TMP"' EXIT

qemu-system-aarch64 \
  -M virt,gic-version=3,acpi=$ACPI \
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
