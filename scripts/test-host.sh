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
  kernel/platform/fdt.cpp kernel/platform/fdt_state.cpp \
  kernel/platform/fdt_property.cpp kernel/process/elf.cpp \
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
  -I kernel/include \
  tests/host/hid_probe.cpp kernel/drivers/input/hid.cpp \
  -o "$BUILD_DIR/hid_probe"
"$BUILD_DIR/hid_probe"

"$CXX" -std=c++20 -O1 -Wall -Wextra -Werror \
  -I libs/core -I kernel/include \
  tests/host/framebuffer_probe.cpp kernel/io/framebuffer_console.cpp \
  kernel/io/framebuffer_draw.cpp kernel/io/font.cpp libs/core/memory.cpp \
  -o "$BUILD_DIR/framebuffer_probe"
"$BUILD_DIR/framebuffer_probe"

"$CXX" -std=c++20 -O1 -Wall -Wextra -Werror \
  -I libs/core -I kernel/include \
  tests/host/fat32_probe.cpp kernel/fs/fat32.cpp libs/core/memory.cpp \
  -o "$BUILD_DIR/fat32_probe"
"$BUILD_DIR/fat32_probe"

"$CXX" -std=c++20 -O1 -Wall -Wextra -Werror \
  -I packages/libgemapi/include -I packages/libkgema/include \
  tests/host/gemapi_fs_probe.cpp packages/libgemapi/src/fs.cpp \
  -o "$BUILD_DIR/gemapi_fs_probe"
"$BUILD_DIR/gemapi_fs_probe"

"$CXX" -std=c++20 -O1 -Wall -Wextra -Werror \
  -I packages/libkgema/include \
  tests/host/kgema_probe.cpp \
  packages/libkgema/src/file.cpp packages/libkgema/src/io.cpp \
  packages/libkgema/src/process.cpp packages/libkgema/src/memory.cpp \
  -o "$BUILD_DIR/kgema_probe"
"$BUILD_DIR/kgema_probe"

"$CXX" -std=c++20 -O1 -Wall -Wextra -Werror \
  -I packages/libgemapi/include -I packages/libkgema/include \
  tests/host/gemapi_api_probe.cpp \
  packages/libgemapi/src/io.cpp packages/libgemapi/src/process.cpp \
  packages/libgemapi/src/memory.cpp packages/libgemapi/src/error.cpp \
  -o "$BUILD_DIR/gemapi_api_probe"
"$BUILD_DIR/gemapi_api_probe"

echo "== gslfs image test =="
"$ROOT/scripts/test-gslfs.sh"
