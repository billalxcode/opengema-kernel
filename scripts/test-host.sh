#!/bin/bash
# Host unit tests for libs/core. Runs with the local clang++ and the system
# standard library, so it works both on macOS and inside the container.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1

if [ -n "${OG_HOST_BUILD_DIR:-}" ]; then
  BUILD_DIR="$OG_HOST_BUILD_DIR"
  mkdir -p "$BUILD_DIR"
else
  BUILD_DIR="$(mktemp -d)"
  trap 'rm -rf "$BUILD_DIR"' EXIT
fi

CXX="${CXX:-clang++}"

build_probe() {
  local name="$1"
  shift
  "$CXX" -std=c++20 -O1 -Wall -Wextra -Werror "$@" -o "$BUILD_DIR/$name"
}

build_probe host_tests -DOG_HOST_TEST -I libs/core -I kernel/include \
  tests/host/main.cpp libs/core/memory.cpp libs/core/format.cpp \
  kernel/fs/partition.cpp kernel/fs/vfs.cpp kernel/fs/fd_table.cpp \
  kernel/platform/fdt.cpp kernel/platform/fdt_state.cpp \
  kernel/platform/fdt_property.cpp kernel/process/elf.cpp
"$BUILD_DIR/host_tests"

if [ "${OG_HOST_CORE_ONLY:-0}" = "1" ]; then
  echo "host tests: core only"
  exit 0
fi

build_probe ramfs_probe -I libs/core -I kernel/include \
  tests/host/ramfs_probe.cpp kernel/fs/ramfs.cpp libs/core/memory.cpp
"$BUILD_DIR/ramfs_probe"

build_probe acpi_probe -I libs/core -I kernel/include \
  tests/host/acpi_probe.cpp kernel/acpi/acpi.cpp libs/core/memory.cpp
"$BUILD_DIR/acpi_probe"

build_probe procfs_probe -I libs/core -I kernel/include \
  tests/host/procfs_probe.cpp kernel/fs/procfs.cpp kernel/acpi/acpi.cpp \
  libs/core/memory.cpp libs/core/format.cpp
"$BUILD_DIR/procfs_probe"

build_probe devfs_probe -I libs/core -I kernel/include \
  tests/host/devfs_probe.cpp kernel/fs/devfs.cpp libs/core/memory.cpp
"$BUILD_DIR/devfs_probe"

build_probe hid_probe -I kernel/include \
  tests/host/hid_probe.cpp kernel/drivers/input/hid.cpp
"$BUILD_DIR/hid_probe"

build_probe vfs_mutation_probe -DOG_HOST_TEST -I libs/core -I kernel/include \
  tests/host/vfs_mutation_probe.cpp kernel/fs/vfs.cpp \
  kernel/fs/fd_table.cpp kernel/fs/ramfs.cpp libs/core/memory.cpp
"$BUILD_DIR/vfs_mutation_probe"

build_probe vfs_mount_probe -DOG_HOST_TEST -I libs/core -I kernel/include \
  tests/host/vfs_mount_probe.cpp kernel/fs/vfs.cpp \
  kernel/fs/fd_table.cpp kernel/fs/ramfs.cpp libs/core/memory.cpp
"$BUILD_DIR/vfs_mount_probe"

build_probe framebuffer_probe -I libs/core -I kernel/include \
  tests/host/framebuffer_probe.cpp kernel/io/framebuffer_console.cpp \
  kernel/io/framebuffer_draw.cpp kernel/io/font.cpp libs/core/memory.cpp
"$BUILD_DIR/framebuffer_probe"

build_probe fat32_probe -I libs/core -I kernel/include \
  tests/host/fat32_probe.cpp kernel/fs/fat32.cpp libs/core/memory.cpp
"$BUILD_DIR/fat32_probe"

build_probe gemapi_fs_probe \
  -I packages/libgemapi/include -I packages/libkgema/include \
  tests/host/gemapi_fs_probe.cpp packages/libgemapi/src/fs.cpp \
  packages/libgemapi/src/error.cpp
"$BUILD_DIR/gemapi_fs_probe"

build_probe kgema_probe -I packages/libkgema/include \
  tests/host/kgema_probe.cpp \
  packages/libkgema/src/file.cpp packages/libkgema/src/io.cpp \
  packages/libkgema/src/process.cpp packages/libkgema/src/memory.cpp
"$BUILD_DIR/kgema_probe"

build_probe gemapi_api_probe \
  -I packages/libgemapi/include -I packages/libkgema/include \
  tests/host/gemapi_api_probe.cpp \
  packages/libgemapi/src/io.cpp packages/libgemapi/src/process.cpp \
  packages/libgemapi/src/memory.cpp packages/libgemapi/src/error.cpp
"$BUILD_DIR/gemapi_api_probe"

echo "== gslfs image test =="
"$ROOT/scripts/test-gslfs.sh"
