#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

podman build -f "$ROOT/Containerfile" -t opengema-build "$ROOT"
rm -rf "$ROOT/build"
podman run --rm -v "$ROOT:/work" -w /work opengema-build \
  cmake -B build --toolchain cmake/toolchain-aarch64-uefi.cmake -G Ninja
podman run --rm -v "$ROOT:/work" -w /work opengema-build cmake --build build

# compile_commands.json is written with container paths (/work).
# Rewrite to host paths so clangd and VS Code IntelliSense resolve includes.
python3 "$ROOT/scripts/fix_compile_commands.py" "$ROOT/build/compile_commands.json" "$ROOT"

ls -lh "$ROOT/build/BOOTAA64.EFI" "$ROOT/build/gema-system.img" "$ROOT/esp/EFI/BOOT/BOOTAA64.EFI"
