#!/bin/bash
# Build esp.img from the CMake system image and the UEFI loader.
# Runs both in the build container and in native CI mode.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1

LOOP=""
PERSIST_TMP="$(mktemp)"
STARTUP_TMP="$(mktemp)"
cleanup() {
  if [ -n "$LOOP" ]; then
    umount /mnt/esp 2>/dev/null || true
    losetup -d "$LOOP" 2>/dev/null || true
  fi
  rm -f "$PERSIST_TMP" "$STARTUP_TMP"
}
trap cleanup EXIT

rm -f esp.img
dd if=/dev/zero of=esp.img bs=1M count=128 status=none
sgdisk -o \
  -n 1:0:+64M -t 1:ef00 -c 1:ESP \
  -n 2:0:+32M -t 2:8300 -c 2:SYSTEM \
  esp.img >/dev/null
START1="$(sgdisk -p esp.img | awk '/^ *1 /{print $2}')"
START2="$(sgdisk -p esp.img | awk '/^ *2 /{print $2}')"
OFFSET1=$((START1 * 512))

if [ "${OG_ESP_NO_LOOP:-0}" != "1" ]; then
  LOOP="$(losetup --show -f -o "$OFFSET1" \
    --sizelimit $((64 * 1024 * 1024)) esp.img 2>/dev/null || true)"
fi

if [ -n "$LOOP" ]; then
  mkfs.vfat -F 32 -n ESP "$LOOP" >/dev/null
  mkdir -p /mnt/esp
  mount "$LOOP" /mnt/esp
  mkdir -p /mnt/esp/EFI/BOOT
  cp esp/EFI/BOOT/BOOTAA64.EFI /mnt/esp/EFI/BOOT/
  dd if=/dev/zero of=/mnt/esp/persist.bin bs=4096 count=1 status=none
  printf '%s\r\n' 'FS0:\EFI\BOOT\BOOTAA64.EFI' > /mnt/esp/startup.nsh
  umount /mnt/esp
  losetup -d "$LOOP"
  LOOP=""
  echo 'esp: loop method ok'
else
  echo 'esp: mtools fallback'
  mformat -i "esp.img@@$OFFSET1" -F -v ESP -T 131072 -c 1 ::
  mmd -i "esp.img@@$OFFSET1" ::/EFI
  mmd -i "esp.img@@$OFFSET1" ::/EFI/BOOT
  mcopy -i "esp.img@@$OFFSET1" esp/EFI/BOOT/BOOTAA64.EFI \
    ::/EFI/BOOT/BOOTAA64.EFI
  dd if=/dev/zero of="$PERSIST_TMP" bs=4096 count=1 status=none
  mcopy -i "esp.img@@$OFFSET1" "$PERSIST_TMP" ::/persist.bin
  printf '%s\r\n' 'FS0:\EFI\BOOT\BOOTAA64.EFI' > "$STARTUP_TMP"
  mcopy -i "esp.img@@$OFFSET1" "$STARTUP_TMP" ::/startup.nsh
  mdir -i "esp.img@@$OFFSET1" ::/EFI/BOOT
  mdir -i "esp.img@@$OFFSET1" ::/
fi

dd if=build/gema-system.img of=esp.img bs=512 seek="$START2" \
  conv=notrunc status=none
ls -lh esp.img
