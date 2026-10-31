# Toolchain for UEFI AArch64 PE build inside podman container.
# Flow: clang (aarch64-unknown-windows, freestanding, nostdinc,
# own libs/cxx headers) -> COFF objects -> lld-link -> BOOTAA64.EFI.
# No objcopy, no ELF tricks. lld-link emits a proper PE with .reloc.
# Usage:
#   cmake -B build --toolchain cmake/toolchain-aarch64-uefi.cmake -G Ninja
# Must run inside container (clang-19 + lld-19).

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

add_compile_options(
  --target=aarch64-unknown-windows
  -ffreestanding
  -nostdinc
)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

find_program(OG_LLD_LINK NAMES lld-link lld-link-19 REQUIRED)
