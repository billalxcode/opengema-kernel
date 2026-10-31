cmake_minimum_required(VERSION 3.25)

project(opengema-kernel LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(objs OBJECT
  kernel/boot/efi_main.cpp
  kernel/arch/aarch64/uart_pl011.cpp
  kernel/arch/aarch64/mmu.cpp
  kernel/kernel/main.cpp
  kernel/kernel/stubs.cpp
)

target_include_directories(objs PRIVATE
  ${CMAKE_SOURCE_DIR}/kernel/include
  ${CMAKE_SOURCE_DIR}/libs/efi
  ${CMAKE_SOURCE_DIR}/libs/cxx
)

target_compile_options(objs PRIVATE
  -Wall -Wextra -Werror -Wconversion -Wpedantic
  -ffreestanding
  -fno-exceptions -fno-rtti
  -fno-threadsafe-statics -fno-unwind-tables
  -fshort-wchar
  -mno-red-zone
)

# Direct PE/COFF link. lld-link emits proper headers + .reloc.
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/BOOTAA64.EFI
  COMMAND ${OG_LLD_LINK}
    /nologo
    /machine:arm64
    /subsystem:efi_application
    /entry:efi_main
    /nodefaultlib
    /out:${CMAKE_BINARY_DIR}/BOOTAA64.EFI
    $<TARGET_OBJECTS:objs>
  COMMAND_EXPAND_LISTS
  DEPENDS objs
  COMMENT "lld-link BOOTAA64.EFI"
)

add_custom_target(BOOTAA64EFI ALL DEPENDS ${CMAKE_BINARY_DIR}/BOOTAA64.EFI)

add_custom_command(TARGET BOOTAA64EFI POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/esp/EFI/BOOT
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/BOOTAA64.EFI ${CMAKE_SOURCE_DIR}/esp/EFI/BOOT/BOOTAA64.EFI
  COMMENT "Copy BOOTAA64.EFI to esp/EFI/BOOT"
)
