#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

if [ "${OG_NATIVE:-0}" = "1" ]; then
  if ! command -v clang++ >/dev/null 2>&1 ||
      ! command -v lld-link >/dev/null 2>&1 ||
      ! command -v aarch64-linux-gnu-ld >/dev/null 2>&1 ||
      ! command -v aarch64-linux-gnu-ar >/dev/null 2>&1; then
    echo "build: native mode needs clang++, lld-link, aarch64-linux-gnu-ld," \
      "aarch64-linux-gnu-ar, cmake, and ninja" >&2
    exit 1
  fi
  rm -rf "$ROOT/build"
  cmake -B "$ROOT/build" \
    --toolchain "$ROOT/cmake/toolchain-aarch64-uefi.cmake" -G Ninja
  cmake --build "$ROOT/build"
else
  podman build -f "$ROOT/Containerfile" -t opengema-build "$ROOT"
  rm -rf "$ROOT/build"
  podman run --rm -v "$ROOT:/work" -w /work opengema-build \
    cmake -B build --toolchain cmake/toolchain-aarch64-uefi.cmake -G Ninja
  podman run --rm -v "$ROOT:/work" -w /work opengema-build cmake --build build
fi

# compile_commands.json is written with container paths (/work).
# Rewrite to host paths so clangd and VS Code IntelliSense resolve includes.
python3 "$ROOT/scripts/fix_compile_commands.py" "$ROOT/build/compile_commands.json" "$ROOT"

ls -lh "$ROOT/build/BOOTAA64.EFI" "$ROOT/build/gema-system.img" "$ROOT/esp/EFI/BOOT/BOOTAA64.EFI"
