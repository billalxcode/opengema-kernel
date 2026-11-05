# Toolchain for the exported opengema userland SDK.
# The SDK targets freestanding AArch64 Linux ELF images and is linked with
# the linker script installed next to this file.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

set(CMAKE_C_FLAGS_INIT "--target=aarch64-linux-gnu")
set(CMAKE_CXX_FLAGS_INIT "--target=aarch64-linux-gnu")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
