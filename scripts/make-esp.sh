#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMG="$ROOT/esp.img"

podman run --rm -v "$ROOT:/work" -w /work opengema-build bash -c "
set -e
apt-get update -qq && apt-get install -y -qq dosfstools mtools gdisk parted >/dev/null 2>&1
rm -f esp.img
dd if=/dev/zero of=esp.img bs=1M count=64 status=none
sgdisk -o -n 1:0:+60M -t 1:ef00 -c 1:ESP esp.img >/dev/null
OFFSET=\$((2048 * 512))
LOOP=\$(losetup --show -f -o \$OFFSET esp.img || true)
if [ -n \"\$LOOP\" ]; then
  mkfs.vfat -F 32 -n ESP \"\$LOOP\" >/dev/null
  mkdir -p /mnt/esp
  mount \"\$LOOP\" /mnt/esp
  mkdir -p /mnt/esp/EFI/BOOT
  cp esp/EFI/BOOT/BOOTAA64.EFI /mnt/esp/EFI/BOOT/
  if [ -f build/gemabox.elf ]; then
    cp build/gemabox.elf /mnt/esp/GEMABOX.ELF
  fi
  printf '%s\r\n' 'FS0:\EFI\BOOT\BOOTAA64.EFI' > /mnt/esp/startup.nsh
  umount /mnt/esp
  losetup -d \"\$LOOP\" || true
  echo 'loop method ok'
else
  echo 'no loop device, use mtools fallback'
  START_SECTOR=\$(sgdisk -p esp.img | awk '/^ *1 /{print \$2}')
  echo \"start sector: \$START_SECTOR\"
  mformat -i esp.img@@\$((\$START_SECTOR * 512)) -F -v ESP ::
  mmd -i esp.img@@\$((\$START_SECTOR * 512)) ::/EFI
  mmd -i esp.img@@\$((\$START_SECTOR * 512)) ::/EFI/BOOT
  mcopy -i esp.img@@\$((\$START_SECTOR * 512)) esp/EFI/BOOT/BOOTAA64.EFI ::/EFI/BOOT/BOOTAA64.EFI
  if [ -f build/gemabox.elf ]; then
    mcopy -i esp.img@@\$((\$START_SECTOR * 512)) build/gemabox.elf ::/GEMABOX.ELF
  fi
  printf '%s\r\n' 'FS0:\EFI\BOOT\BOOTAA64.EFI' > /tmp/startup.nsh
  mcopy -i esp.img@@\$((\$START_SECTOR * 512)) /tmp/startup.nsh ::/startup.nsh
  mdir -i esp.img@@\$((\$START_SECTOR * 512)) ::/EFI/BOOT
  mdir -i esp.img@@\$((\$START_SECTOR * 512)) ::/
fi
ls -lh esp.img
"
ls -lh "$IMG"
