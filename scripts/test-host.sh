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
  kernel/fs/partition.cpp kernel/fs/vfs.cpp kernel/fs/fd_table.cpp \
  kernel/platform/fdt.cpp kernel/process/elf.cpp \
  -o "$BUILD_DIR/host_tests"

"$BUILD_DIR/host_tests"

"$CXX" -std=c++20 -O1 -Wall -Wextra -Werror \
  -I libs/core -I kernel/include \
  tests/host/ramfs_probe.cpp kernel/fs/ramfs.cpp libs/core/memory.cpp \
  -o "$BUILD_DIR/ramfs_probe"
"$BUILD_DIR/ramfs_probe"

"$CXX" -std=c++20 -O1 -Wall -Wextra -Werror \
  -I libs/core -I kernel/include \
  tests/host/procfs_probe.cpp kernel/fs/procfs.cpp libs/core/memory.cpp \
  libs/core/format.cpp \
  -o "$BUILD_DIR/procfs_probe"
"$BUILD_DIR/procfs_probe"

"$CXX" -std=c++20 -O1 -Wall -Wextra -Werror \
  -I libs/core -I kernel/include \
  tests/host/devfs_probe.cpp kernel/fs/devfs.cpp libs/core/memory.cpp \
  -o "$BUILD_DIR/devfs_probe"
"$BUILD_DIR/devfs_probe"

"$CXX" -std=c++20 -O1 -Wall -Wextra -Werror \
  -I libs/core -I kernel/include \
  tests/host/fat32_probe.cpp kernel/fs/fat32.cpp libs/core/memory.cpp \
  -o "$BUILD_DIR/fat32_probe"
"$BUILD_DIR/fat32_probe"

echo "== gslfs image test =="
"$ROOT/scripts/test-gslfs.sh"
