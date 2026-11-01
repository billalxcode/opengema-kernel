cmake_minimum_required(VERSION 3.25)

project(opengema-kernel LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include roots. Each module picks only what its layer is allowed to use.
add_library(og_inc_cxx INTERFACE)
target_include_directories(og_inc_cxx INTERFACE
  ${CMAKE_SOURCE_DIR}/libs/cxx)

add_library(og_inc_core INTERFACE)
target_include_directories(og_inc_core INTERFACE
  ${CMAKE_SOURCE_DIR}/libs/core)
target_link_libraries(og_inc_core INTERFACE og_inc_cxx)

add_library(og_inc_kernel INTERFACE)
target_include_directories(og_inc_kernel INTERFACE
  ${CMAKE_SOURCE_DIR}/kernel/include)
target_link_libraries(og_inc_kernel INTERFACE og_inc_cxx)

add_library(og_inc_efi INTERFACE)
target_include_directories(og_inc_efi INTERFACE
  ${CMAKE_SOURCE_DIR}/libs/efi)
target_link_libraries(og_inc_efi INTERFACE og_inc_cxx)

set(OG_CXX_FLAGS
  -Wall -Wextra -Werror -Wconversion -Wpedantic
  -ffreestanding
  -fno-exceptions -fno-rtti
  -fno-threadsafe-statics -fno-unwind-tables
  -fshort-wchar
  -mno-red-zone
)

# One OBJECT library per module. DEPS lists lower layers only, so the
# link graph mirrors the architecture in docs/ROADMAP.md.
function(og_add_module name)
  cmake_parse_arguments(arg "" "" "SOURCES;DEPS" ${ARGN})
  add_library(${name} OBJECT ${arg_SOURCES})
  target_compile_options(${name} PRIVATE ${OG_CXX_FLAGS})
  target_link_libraries(${name} PUBLIC ${arg_DEPS})
endfunction()

og_add_module(og_lib_core
  SOURCES libs/core/memory.cpp libs/core/format.cpp
  DEPS og_inc_core)

og_add_module(og_arch
  SOURCES kernel/arch/aarch64/mmu.cpp
  DEPS og_lib_core og_inc_kernel)

og_add_module(og_io
  SOURCES kernel/io/mmio.cpp kernel/io/console.cpp kernel/io/print.cpp
  DEPS og_arch og_inc_kernel)

og_add_module(og_core
  SOURCES kernel/core/log.cpp kernel/core/panic.cpp
  DEPS og_io og_inc_kernel)

og_add_module(og_drivers
  SOURCES kernel/drivers/uart/pl011.cpp
  DEPS og_core og_inc_kernel)

og_add_module(og_kernel
  SOURCES kernel/core/init.cpp
  DEPS og_drivers og_inc_kernel)

og_add_module(og_boot
  SOURCES
    kernel/boot/uefi/efi_main.cpp
    kernel/boot/uefi/efi_console.cpp
    kernel/boot/uefi/efi_memmap.cpp
    kernel/boot/uefi/efi_exit.cpp
  DEPS og_kernel og_inc_efi og_inc_kernel)

# Direct PE/COFF link. lld-link emits proper headers and .reloc.
set(OG_IMAGE_OBJECTS
  $<TARGET_OBJECTS:og_boot>
  $<TARGET_OBJECTS:og_kernel>
  $<TARGET_OBJECTS:og_drivers>
  $<TARGET_OBJECTS:og_core>
  $<TARGET_OBJECTS:og_io>
  $<TARGET_OBJECTS:og_arch>
  $<TARGET_OBJECTS:og_lib_core>)

add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/BOOTAA64.EFI
  COMMAND ${OG_LLD_LINK}
    /nologo
    /machine:arm64
    /subsystem:efi_application
    /entry:efi_main
    /nodefaultlib
    /out:${CMAKE_BINARY_DIR}/BOOTAA64.EFI
    ${OG_IMAGE_OBJECTS}
  COMMAND_EXPAND_LISTS
  DEPENDS og_boot og_kernel og_drivers og_core og_io og_arch og_lib_core
  COMMENT "lld-link BOOTAA64.EFI"
)

add_custom_target(BOOTAA64EFI ALL DEPENDS ${CMAKE_BINARY_DIR}/BOOTAA64.EFI)

# Architecture guard: fails the build on layering or duplication violations.
add_custom_target(og_arch_check ALL
  COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/check-arch.sh
  COMMENT "check architecture rules"
  VERBATIM)

add_dependencies(BOOTAA64EFI og_arch_check)

add_custom_command(TARGET BOOTAA64EFI POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/esp/EFI/BOOT
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/BOOTAA64.EFI ${CMAKE_SOURCE_DIR}/esp/EFI/BOOT/BOOTAA64.EFI
  COMMENT "Copy BOOTAA64.EFI to esp/EFI/BOOT"
)
