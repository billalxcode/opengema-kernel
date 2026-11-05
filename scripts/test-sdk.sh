#!/bin/bash
# Build the exported SDK and compile the sample application against it.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1

if [ ! -f build/libgemapi.a ] || [ ! -f build/libkgema.a ]; then
  echo "sdk probe: build the kernel first with ./scripts/build.sh"
  exit 1
fi

PREFIX="$(mktemp -d)"
BUILD_DIR="$(mktemp -d)"
cleanup() {
  rm -rf "$PREFIX" "$BUILD_DIR"
}
trap cleanup EXIT

cmake --install build --prefix "$PREFIX" >/dev/null

TOOLCHAIN="$(find "$PREFIX" -name opengema-sdk-toolchain.cmake -print -quit)"
if [ -z "$TOOLCHAIN" ]; then
  echo "sdk probe: toolchain file missing"
  exit 1
fi

cmake -S examples/hello -B "$BUILD_DIR/hello" \
  --toolchain "$TOOLCHAIN" \
  -DCMAKE_PREFIX_PATH="$PREFIX" \
  -G Ninja >/dev/null
cmake --build "$BUILD_DIR/hello" >/dev/null

ELF="$BUILD_DIR/hello/hello"
if [ ! -f "$ELF" ]; then
  echo "sdk probe: example binary missing"
  exit 1
fi
if ! readelf -h "$ELF" | grep -q "AArch64"; then
  echo "sdk probe: example is not AArch64"
  exit 1
fi

echo "sdk probe: OK"
