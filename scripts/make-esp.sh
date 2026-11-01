#!/bin/bash
# Build the boot disk: GPT with an EFI System Partition and a GSLFS system
# partition. The system image is produced by the CMake build.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMG="$ROOT/esp.img"
SYSTEM="$ROOT/build/gema-system.img"

if [ ! -f "$SYSTEM" ]; then
  echo "missing build/gema-system.img, run ./scripts/build.sh first"
  exit 1
fi

SYSTEM_BYTES="$(wc -c < "$SYSTEM" | tr -d ' ')"
if [ "$SYSTEM_BYTES" -gt $((32 * 1024 * 1024)) ]; then
  echo "system image is larger than the 32 MiB partition"
  exit 1
fi

podman run --rm -v "$ROOT:/work" -w /work opengema-build bash -c "
set -e
apt-get update -qq && apt-get install -y -qq dosfstools mtools gdisk parted >/dev/null 2>&1
rm -f esp.img
dd if=/dev/zero of=esp.img bs=1M count=128 status=none
sgdisk -o \
  -n 1:0:+64M -t 1:ef00 -c 1:ESP \
  -n 2:0:+32M -t 2:8300 -c 2:SYSTEM \
  esp.img >/dev/null
START1=\$(sgdisk -p esp.img | awk '/^ *1 /{print \$2}')
START2=\$(sgdisk -p esp.img | awk '/^ *2 /{print \$2}')
OFFSET1=\$((START1 * 512))
LOOP=\$(losetup --show -f -o \$OFFSET1 --sizelimit \$((64 * 1024 * 1024)) esp.img || true)
if [ -n \"\$LOOP\" ]; then
  mkfs.vfat -F 32 -n ESP \"\$LOOP\" >/dev/null
  mkdir -p /mnt/esp
  mount \"\$LOOP\" /mnt/esp
  mkdir -p /mnt/esp/EFI/BOOT
  cp esp/EFI/BOOT/BOOTAA64.EFI /mnt/esp/EFI/BOOT/
  printf '%s\r\n' 'FS0:\EFI\BOOT\BOOTAA64.EFI' > /mnt/esp/startup.nsh
  umount /mnt/esp
  losetup -d \"\$LOOP\" || true
  echo 'loop method ok'
else
  echo 'no loop device, use mtools fallback'
  mformat -i esp.img@@\$OFFSET1 -F -v ESP -T 131072 -c 1 ::
  mmd -i esp.img@@\$OFFSET1 ::/EFI
  mmd -i esp.img@@\$OFFSET1 ::/EFI/BOOT
  mcopy -i esp.img@@\$OFFSET1 esp/EFI/BOOT/BOOTAA64.EFI ::/EFI/BOOT/BOOTAA64.EFI
  printf '%s\r\n' 'FS0:\EFI\BOOT\BOOTAA64.EFI' > /tmp/startup.nsh
  mcopy -i esp.img@@\$OFFSET1 /tmp/startup.nsh ::/startup.nsh
  mdir -i esp.img@@\$OFFSET1 ::/EFI/BOOT
  mdir -i esp.img@@\$OFFSET1 ::/
fi
dd if=build/gema-system.img of=esp.img bs=512 seek=\$START2 conv=notrunc status=none
ls -lh esp.img
"
ls -lh "$IMG"
