#!/bin/bash
# Host unit tests for libs/core. Runs with the local clang++ and the system
# standard library, so it works both on macOS and inside the container.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1

BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT

CXX="${CXX:-clang++}"

"$CXX" -std=c++20 -O1 -Wall -Wextra -Werror \
  -I libs/core -I kernel/include \
  tests/host/main.cpp libs/core/memory.cpp libs/core/format.cpp \
  kernel/fs/partition.cpp \
  -o "$BUILD_DIR/host_tests"

"$BUILD_DIR/host_tests"

echo "== gslfs image test =="
"$ROOT/scripts/test-gslfs.sh"
