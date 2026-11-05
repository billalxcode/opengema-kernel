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

if [ "${OG_NATIVE:-0}" = "1" ]; then
  OG_ESP_NO_LOOP="${OG_ESP_NO_LOOP:-1}" \
    bash "$ROOT/scripts/make-esp-disk.sh"
else
  podman run --rm -v "$ROOT:/work" -w /work opengema-build \
    bash scripts/make-esp-disk.sh
fi
ls -lh "$IMG"
