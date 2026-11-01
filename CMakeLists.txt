cmake_minimum_required(VERSION 3.25)

project(opengema-kernel LANGUAGES C CXX ASM)

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

# gemabox: the base command program for user mode. Assembled, linked at
# its virtual address, then flattened to a raw binary for the ESP.
find_program(OG_GEMABOX_LD NAMES aarch64-linux-gnu-ld REQUIRED)
find_program(OG_GEMABOX_OBJCOPY NAMES aarch64-linux-gnu-objcopy REQUIRED)
set(OG_GEMABOX_OBJECT ${CMAKE_BINARY_DIR}/gemabox.o)
set(OG_GEMABOX_ELF ${CMAKE_BINARY_DIR}/gemabox.elf)
set(OG_GEMABOX_BIN ${CMAKE_BINARY_DIR}/gemabox.bin)

add_custom_command(
  OUTPUT ${OG_GEMABOX_BIN}
  COMMAND ${CMAKE_C_COMPILER} --target=aarch64-linux-gnu -ffreestanding -nostdlib
          -c ${CMAKE_SOURCE_DIR}/pubs/gemabox/main.S -o ${OG_GEMABOX_OBJECT}
  COMMAND ${OG_GEMABOX_LD} -Ttext=0x100000000 -e _start ${OG_GEMABOX_OBJECT}
          -o ${OG_GEMABOX_ELF}
  COMMAND ${OG_GEMABOX_OBJCOPY} -O binary ${OG_GEMABOX_ELF} ${OG_GEMABOX_BIN}
  DEPENDS ${CMAKE_SOURCE_DIR}/pubs/gemabox/main.S
  COMMENT "build gemabox")

add_custom_target(og_gemabox DEPENDS ${OG_GEMABOX_BIN})

# One OBJECT library per module. DEPS lists lower layers only, so the
# link graph mirrors the architecture in docs/ROADMAP.md.
function(og_add_module name)
  cmake_parse_arguments(arg "" "" "SOURCES;DEPS" ${ARGN})
  add_library(${name} OBJECT ${arg_SOURCES})
  target_compile_options(${name} PRIVATE
    $<$<COMPILE_LANGUAGE:C,CXX>:--target=aarch64-unknown-windows>
    $<$<COMPILE_LANGUAGE:C,CXX>:-ffreestanding>
    $<$<COMPILE_LANGUAGE:C,CXX>:-nostdinc>
    $<$<COMPILE_LANGUAGE:C,CXX>:-Wall>
    $<$<COMPILE_LANGUAGE:C,CXX>:-Wextra>
    $<$<COMPILE_LANGUAGE:C,CXX>:-Werror>
    $<$<COMPILE_LANGUAGE:C,CXX>:-Wconversion>
    $<$<COMPILE_LANGUAGE:C,CXX>:-Wpedantic>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-unwind-tables>
    $<$<COMPILE_LANGUAGE:CXX>:-fshort-wchar>
    $<$<COMPILE_LANGUAGE:CXX>:-mno-red-zone>
    $<$<COMPILE_LANGUAGE:CXX>:-mgeneral-regs-only>
    $<$<COMPILE_LANGUAGE:CXX>:-fstack-protector-strong>
    $<$<COMPILE_LANGUAGE:ASM>:--target=aarch64-unknown-windows>)
  target_link_libraries(${name} PUBLIC ${arg_DEPS})
endfunction()

og_add_module(og_lib_core
  SOURCES libs/core/memory.cpp libs/core/format.cpp
  DEPS og_inc_core)

og_add_module(og_arch
  SOURCES
    kernel/arch/aarch64/mmu.cpp
    kernel/arch/aarch64/vectors.S
    kernel/arch/aarch64/context.S
  DEPS og_lib_core og_inc_kernel)

og_add_module(og_io
  SOURCES
    kernel/io/mmio.cpp
    kernel/io/console.cpp
    kernel/io/print.cpp
    kernel/io/framebuffer.cpp
  DEPS og_arch og_inc_kernel)

og_add_module(og_core
  SOURCES kernel/core/log.cpp kernel/core/panic.cpp kernel/core/stack_guard.cpp
  DEPS og_io og_inc_kernel)

og_add_module(og_mem
  SOURCES
    kernel/mem/pmm.cpp
    kernel/mem/vmm.cpp
    kernel/mem/heap.cpp
    kernel/mem/self_test.cpp
  DEPS og_core og_inc_kernel)

og_add_module(og_events
  SOURCES
    kernel/events/irq.cpp
    kernel/events/trap.cpp
    kernel/events/workqueue.cpp
    kernel/events/self_test.cpp
  DEPS og_core og_inc_kernel)

og_add_module(og_dma
  SOURCES kernel/dma/dma.cpp kernel/dma/pool.cpp kernel/dma/self_test.cpp
  DEPS og_mem og_inc_kernel)

og_add_module(og_time
  SOURCES kernel/time/timer.cpp kernel/time/delay.cpp
  DEPS og_events og_inc_kernel)

og_add_module(og_drivers
  SOURCES
    kernel/drivers/device.cpp
    kernel/drivers/uart/pl011.cpp
    kernel/drivers/irq/gicv3.cpp
    kernel/drivers/rtc/pl031.cpp
    kernel/drivers/pci/ecam.cpp
    kernel/drivers/virtio/virtio_common.cpp
    kernel/drivers/virtio/virtio_blk.cpp
    kernel/drivers/virtio/virtio_gpu.cpp
  DEPS og_events og_time og_dma og_core og_inc_kernel)

og_add_module(og_fs
  SOURCES
    kernel/fs/vfs.cpp
    kernel/fs/devfs.cpp
    kernel/fs/ramfs.cpp
    kernel/fs/procfs.cpp
    kernel/fs/partition.cpp
    kernel/fs/fat32.cpp
  DEPS og_sched og_mem og_core og_inc_kernel)

og_add_module(og_sched
  SOURCES kernel/sched/scheduler.cpp
  DEPS og_events og_time og_mem og_core og_inc_kernel)

og_add_module(og_syscall
  SOURCES kernel/syscall/syscall.cpp
  DEPS og_sched og_events og_mem og_core og_inc_kernel)

og_add_module(og_sync
  SOURCES kernel/sync/sync.cpp
  DEPS og_sched og_arch og_core og_inc_kernel)

og_add_module(og_platform
  SOURCES
    kernel/platform/qemu_virt.cpp
    kernel/platform/fdt.cpp
    kernel/platform/psci.cpp
    kernel/platform/smp.cpp
    kernel/platform/secondary.S
  DEPS og_drivers og_events og_time og_sched og_mem og_core og_inc_kernel)

og_add_module(og_kernel
  SOURCES kernel/core/init.cpp
  DEPS og_sync og_platform og_syscall og_sched og_fs og_mem og_events og_time og_dma og_drivers og_inc_kernel)

og_add_module(og_boot
  SOURCES
    kernel/boot/uefi/efi_main.cpp
    kernel/boot/uefi/efi_console.cpp
    kernel/boot/uefi/efi_memmap.cpp
    kernel/boot/uefi/efi_image.cpp
    kernel/boot/uefi/efi_exit.cpp
  DEPS og_kernel og_inc_efi og_inc_kernel)

add_dependencies(og_kernel og_gemabox)

# Direct PE/COFF link. lld-link emits proper headers and .reloc.
set(OG_IMAGE_OBJECTS
  $<TARGET_OBJECTS:og_boot>
  $<TARGET_OBJECTS:og_kernel>
  $<TARGET_OBJECTS:og_drivers>
  $<TARGET_OBJECTS:og_fs>
  $<TARGET_OBJECTS:og_sched>
  $<TARGET_OBJECTS:og_sync>
  $<TARGET_OBJECTS:og_syscall>
  $<TARGET_OBJECTS:og_platform>
  $<TARGET_OBJECTS:og_time>
  $<TARGET_OBJECTS:og_dma>
  $<TARGET_OBJECTS:og_events>
  $<TARGET_OBJECTS:og_mem>
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
  DEPENDS og_boot og_kernel og_drivers og_fs og_sched og_sync og_syscall og_platform og_time og_dma og_events og_mem og_core og_io og_arch og_lib_core
  COMMENT "lld-link BOOTAA64.EFI"
)

add_custom_target(BOOTAA64EFI ALL DEPENDS ${CMAKE_BINARY_DIR}/BOOTAA64.EFI)

# Host unit tests for pure libs/core code. Runs on the build machine, so it
# works inside the container and on macOS.
add_custom_target(og_host_test
  COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test-host.sh
  COMMENT "run host unit tests"
  VERBATIM)

enable_testing()
add_test(NAME host_tests COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test-host.sh)

# Architecture guard: fails the build on layering or duplication violations.
add_custom_target(og_arch_check ALL
  COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/check-arch.sh
  COMMENT "check architecture rules"
  VERBATIM)

add_dependencies(BOOTAA64EFI og_arch_check)

add_custom_command(TARGET BOOTAA64EFI POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/esp/EFI/BOOT
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/BOOTAA64.EFI ${CMAKE_SOURCE_DIR}/esp/EFI/BOOT/BOOTAA64.EFI
  COMMENT "Copy BOOTAA64.EFI to esp/EFI/BOOT")
