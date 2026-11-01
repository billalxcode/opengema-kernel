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

# libgsl owns the fixed filesystem mapping shared by kernel and user space.
add_library(og_inc_gsl INTERFACE)
target_include_directories(og_inc_gsl INTERFACE
  ${CMAKE_SOURCE_DIR}/packages/libgsl/include)

add_library(og_inc_kernel INTERFACE)
target_include_directories(og_inc_kernel INTERFACE
  ${CMAKE_SOURCE_DIR}/kernel/include)
target_link_libraries(og_inc_kernel INTERFACE og_inc_cxx og_inc_gsl)

add_library(og_inc_efi INTERFACE)
target_include_directories(og_inc_efi INTERFACE
  ${CMAKE_SOURCE_DIR}/libs/efi)
target_link_libraries(og_inc_efi INTERFACE og_inc_cxx)

# User space packages. Each layer has its own include root:
#   libkgema   kernel ABI, private to libgemapi
#   libgemapi  public API, the only library an application links
#   gemabox    application
# An application link line names libgemapi only; libgemapi privately carries
# the kgema objects in its static archive and depends on libkgema.so in its
# shared form.
find_program(OG_GEMABOX_LD NAMES aarch64-linux-gnu-ld REQUIRED)
find_program(OG_KGEMA_AR NAMES aarch64-linux-gnu-ar REQUIRED)
find_program(OG_PYTHON NAMES python3 REQUIRED)

set(OG_USER_CXX_FLAGS
  --target=aarch64-linux-gnu
  -std=c++20
  -O2
  -ffreestanding
  -nostdlib
  -nostdinc
  -I${CMAKE_SOURCE_DIR}/packages/libgsl/include
  -mgeneral-regs-only
  -fno-exceptions
  -fno-rtti
  -fno-threadsafe-statics
  -fno-stack-protector
  -fno-builtin
  -fno-unwind-tables
  -fno-asynchronous-unwind-tables
  -fno-use-cxa-atexit
  -Wall
  -Wextra
  -Werror)

set(OG_KGEMA_INCLUDE ${CMAKE_SOURCE_DIR}/packages/libkgema/include)
set(OG_GEMAPI_INCLUDE ${CMAKE_SOURCE_DIR}/packages/libgemapi/include)
set(OG_KGEMA_ARCHIVE ${CMAKE_BINARY_DIR}/libkgema.a)
set(OG_GEMAPI_ARCHIVE ${CMAKE_BINARY_DIR}/libgemapi.a)

set(OG_KGEMA_SOURCES
  packages/libkgema/src/syscall.cpp
  packages/libkgema/src/io.cpp
  packages/libkgema/src/file.cpp
  packages/libkgema/src/memory.cpp
  packages/libkgema/src/process.cpp
  packages/libkgema/src/runtime.cpp)

set(OG_GEMAPI_SOURCES
  packages/libgemapi/src/error.cpp
  packages/libgemapi/src/io.cpp
  packages/libgemapi/src/fs.cpp
  packages/libgemapi/src/memory.cpp
  packages/libgemapi/src/process.cpp)

add_library(og_kgema_user OBJECT ${OG_KGEMA_SOURCES})
target_include_directories(og_kgema_user PRIVATE ${OG_KGEMA_INCLUDE})
target_compile_options(og_kgema_user PRIVATE ${OG_USER_CXX_FLAGS})

add_library(og_gemapi_user OBJECT ${OG_GEMAPI_SOURCES})
target_include_directories(og_gemapi_user PRIVATE
  ${OG_GEMAPI_INCLUDE}
  ${OG_KGEMA_INCLUDE})
target_compile_options(og_gemapi_user PRIVATE ${OG_USER_CXX_FLAGS})

# The process entry point belongs to the API runtime, not to the ABI layer.
add_library(og_gemapi_start OBJECT packages/libgemapi/src/start.cpp)
target_include_directories(og_gemapi_start PRIVATE ${OG_GEMAPI_INCLUDE})
target_compile_options(og_gemapi_start PRIVATE ${OG_USER_CXX_FLAGS})

add_custom_command(
  OUTPUT ${OG_KGEMA_ARCHIVE}
  COMMAND ${CMAKE_COMMAND} -E rm -f ${OG_KGEMA_ARCHIVE}
  COMMAND ${OG_KGEMA_AR} rcs ${OG_KGEMA_ARCHIVE} $<TARGET_OBJECTS:og_kgema_user>
  DEPENDS og_kgema_user
  COMMAND_EXPAND_LISTS
  COMMENT "archive libkgema")

add_custom_target(og_libkgema DEPENDS ${OG_KGEMA_ARCHIVE})

# libgemapi.a is a single link unit: application archive, runtime entry, and
# the private kgema ABI objects. An application never sees its own copy of
# the API boundary in the link line.
add_custom_command(
  OUTPUT ${OG_GEMAPI_ARCHIVE}
  COMMAND ${CMAKE_COMMAND} -E rm -f ${OG_GEMAPI_ARCHIVE}
  COMMAND ${OG_KGEMA_AR} rcs ${OG_GEMAPI_ARCHIVE}
          $<TARGET_OBJECTS:og_gemapi_user>
          $<TARGET_OBJECTS:og_gemapi_start>
          $<TARGET_OBJECTS:og_kgema_user>
  DEPENDS og_gemapi_user og_gemapi_start og_kgema_user
  COMMAND_EXPAND_LISTS
  COMMENT "archive libgemapi")

add_custom_target(og_libgemapi DEPENDS ${OG_GEMAPI_ARCHIVE})

# Shared objects for the dynamic GSL target. They are built now so the ABI
# and the dependency edge are checked on every build; ld-gem loads them in
# GSL-2.
add_library(og_kgema_pic OBJECT ${OG_KGEMA_SOURCES})
target_include_directories(og_kgema_pic PRIVATE ${OG_KGEMA_INCLUDE})
target_compile_options(og_kgema_pic PRIVATE ${OG_USER_CXX_FLAGS} -fPIC)

add_library(og_gemapi_pic OBJECT ${OG_GEMAPI_SOURCES})
target_include_directories(og_gemapi_pic PRIVATE
  ${OG_GEMAPI_INCLUDE}
  ${OG_KGEMA_INCLUDE})
target_compile_options(og_gemapi_pic PRIVATE ${OG_USER_CXX_FLAGS} -fPIC)

set(OG_KGEMA_SO ${CMAKE_BINARY_DIR}/libkgema.so.1)
set(OG_GEMAPI_SO ${CMAKE_BINARY_DIR}/libgemapi.so.1)

add_custom_command(
  OUTPUT ${OG_KGEMA_SO}
  COMMAND ${OG_GEMABOX_LD} -shared -soname libkgema.so.1
          -z max-page-size=4096 -s
          -o ${OG_KGEMA_SO} $<TARGET_OBJECTS:og_kgema_pic>
  COMMAND ${CMAKE_COMMAND} -E create_symlink libkgema.so.1
          ${CMAKE_BINARY_DIR}/libkgema.so
  DEPENDS og_kgema_pic
  COMMAND_EXPAND_LISTS
  COMMENT "link libkgema.so")

add_custom_command(
  OUTPUT ${OG_GEMAPI_SO}
  COMMAND ${OG_GEMABOX_LD} -shared -soname libgemapi.so.1
          -z max-page-size=4096 -s
          -o ${OG_GEMAPI_SO} $<TARGET_OBJECTS:og_gemapi_pic>
          -L${CMAKE_BINARY_DIR} -lkgema
  COMMAND ${CMAKE_COMMAND} -E create_symlink libgemapi.so.1
          ${CMAKE_BINARY_DIR}/libgemapi.so
  DEPENDS og_gemapi_pic ${OG_KGEMA_SO}
  COMMAND_EXPAND_LISTS
  COMMENT "link libgemapi.so")

add_custom_target(og_shared_libraries ALL
  DEPENDS ${OG_KGEMA_SO} ${OG_GEMAPI_SO})

add_library(og_gemabox_user OBJECT packages/gemabox/main.cpp)
target_include_directories(og_gemabox_user PRIVATE ${OG_GEMAPI_INCLUDE})
target_compile_options(og_gemabox_user PRIVATE ${OG_USER_CXX_FLAGS})

set(OG_GEMABOX_ELF ${CMAKE_BINARY_DIR}/gemabox.elf)
set(OG_GEMABOX_STATIC ${CMAKE_BINARY_DIR}/gemabox-static.elf)

# The dynamic executable links libgemapi.so and carries the interpreter path.
# Its own image provides _start from the API runtime object.
add_custom_command(
  OUTPUT ${OG_GEMABOX_ELF}
  COMMAND ${OG_GEMABOX_LD} -T ${CMAKE_SOURCE_DIR}/packages/libgemapi/link.ld
          -z max-page-size=4096 -s
          --dynamic-linker=/system/boot/ld-gem
          -u _start
          $<TARGET_OBJECTS:og_gemabox_user>
          $<TARGET_OBJECTS:og_gemapi_start>
          -L${CMAKE_BINARY_DIR} -rpath-link=${CMAKE_BINARY_DIR} -lgemapi
          -o ${OG_GEMABOX_ELF}
  DEPENDS
    og_gemabox_user
    og_gemapi_start
    ${OG_GEMAPI_SO}
    ${CMAKE_SOURCE_DIR}/packages/libgemapi/link.ld
  COMMAND_EXPAND_LISTS
  COMMENT "link gemabox dynamic")

# Bootstrap fallback and recovery image. Not copied to the ESP by default.
add_custom_command(
  OUTPUT ${OG_GEMABOX_STATIC}
  COMMAND ${OG_GEMABOX_LD} -T ${CMAKE_SOURCE_DIR}/packages/libgemapi/link.ld
          -z max-page-size=4096 -s
          -u _start $<TARGET_OBJECTS:og_gemabox_user> ${OG_GEMAPI_ARCHIVE}
          -o ${OG_GEMABOX_STATIC}
  DEPENDS
    og_gemabox_user
    og_libgemapi
    ${CMAKE_SOURCE_DIR}/packages/libgemapi/link.ld
  COMMAND_EXPAND_LISTS
  COMMENT "link gemabox static")

add_custom_target(og_gemabox_static DEPENDS ${OG_GEMABOX_STATIC})

add_custom_target(og_gemabox DEPENDS ${OG_GEMABOX_ELF})
add_dependencies(og_gemabox og_libgemapi og_gemabox_static)

# ld-gem is the only long lived static program: it must run before any shared
# object is available. It links the private ABI only.
set(OG_LDGEM_INCLUDE ${CMAKE_SOURCE_DIR}/packages/ld-gem/include)
add_library(og_ldgem_user OBJECT
  packages/ld-gem/src/elf.cpp
  packages/ld-gem/src/image.cpp
  packages/ld-gem/src/linker.cpp
  packages/ld-gem/src/main.cpp
  packages/ld-gem/src/start.cpp
  packages/ld-gem/src/text.cpp)
target_include_directories(og_ldgem_user PRIVATE
  ${OG_LDGEM_INCLUDE}
  ${OG_KGEMA_INCLUDE})
target_compile_options(og_ldgem_user PRIVATE ${OG_USER_CXX_FLAGS})

set(OG_LDGEM_ELF ${CMAKE_BINARY_DIR}/ld-gem.elf)
add_custom_command(
  OUTPUT ${OG_LDGEM_ELF}
  COMMAND ${OG_GEMABOX_LD} -T ${CMAKE_SOURCE_DIR}/packages/ld-gem/link.ld
          -z max-page-size=4096 -s
          -u _start $<TARGET_OBJECTS:og_ldgem_user> ${OG_KGEMA_ARCHIVE}
          -o ${OG_LDGEM_ELF}
  DEPENDS
    og_ldgem_user
    og_libkgema
    ${CMAKE_SOURCE_DIR}/packages/ld-gem/link.ld
  COMMAND_EXPAND_LISTS
  COMMENT "link ld-gem")

add_custom_target(og_ld_gem DEPENDS ${OG_LDGEM_ELF})
add_dependencies(og_ld_gem og_libkgema)

# init is the PID 1 user program. It is dynamic like other applications but
# lives in /system/boot because the kernel starts it directly.
add_library(og_init_user OBJECT packages/init/main.cpp)
target_include_directories(og_init_user PRIVATE ${OG_GEMAPI_INCLUDE})
target_compile_options(og_init_user PRIVATE ${OG_USER_CXX_FLAGS})

set(OG_INIT_ELF ${CMAKE_BINARY_DIR}/init.elf)
add_custom_command(
  OUTPUT ${OG_INIT_ELF}
  COMMAND ${OG_GEMABOX_LD} -T ${CMAKE_SOURCE_DIR}/packages/libgemapi/link.ld
          -z max-page-size=4096 -s
          --dynamic-linker=/system/boot/ld-gem
          -u _start
          $<TARGET_OBJECTS:og_init_user>
          $<TARGET_OBJECTS:og_gemapi_start>
          -L${CMAKE_BINARY_DIR} -rpath-link=${CMAKE_BINARY_DIR} -lgemapi
          -o ${OG_INIT_ELF}
  DEPENDS
    og_init_user
    og_gemapi_start
    ${OG_GEMAPI_SO}
    ${CMAKE_SOURCE_DIR}/packages/libgemapi/link.ld
  COMMAND_EXPAND_LISTS
  COMMENT "link init")

add_custom_target(og_init DEPENDS ${OG_INIT_ELF})
add_dependencies(og_init og_libgemapi)

# hello is the first external application used to test command lookup from
# the shell.
add_library(og_hello_user OBJECT packages/hello/main.cpp)
target_include_directories(og_hello_user PRIVATE ${OG_GEMAPI_INCLUDE})
target_compile_options(og_hello_user PRIVATE ${OG_USER_CXX_FLAGS})

set(OG_HELLO_ELF ${CMAKE_BINARY_DIR}/hello.elf)
add_custom_command(
  OUTPUT ${OG_HELLO_ELF}
  COMMAND ${OG_GEMABOX_LD} -T ${CMAKE_SOURCE_DIR}/packages/libgemapi/link.ld
          -z max-page-size=4096 -s
          --dynamic-linker=/system/boot/ld-gem
          -u _start
          $<TARGET_OBJECTS:og_hello_user>
          $<TARGET_OBJECTS:og_gemapi_start>
          -L${CMAKE_BINARY_DIR} -rpath-link=${CMAKE_BINARY_DIR} -lgemapi
          -o ${OG_HELLO_ELF}
  DEPENDS
    og_hello_user
    og_gemapi_start
    ${OG_GEMAPI_SO}
    ${CMAKE_SOURCE_DIR}/packages/libgemapi/link.ld
  COMMAND_EXPAND_LISTS
  COMMENT "link hello")

add_custom_target(og_hello DEPENDS ${OG_HELLO_ELF})
add_dependencies(og_hello og_libgemapi)

# System image for GSLFS. The staging tree mirrors the Unix-like layout in
# docs/gsl/03-filesystem-layout.md.
set(OG_SYSTEM_ROOT ${CMAKE_BINARY_DIR}/system-root)
set(OG_SYSTEM_IMAGE ${CMAKE_BINARY_DIR}/gema-system.img)

add_custom_command(
  OUTPUT ${OG_SYSTEM_IMAGE}
  COMMAND ${CMAKE_COMMAND} -E rm -rf ${OG_SYSTEM_ROOT}
  COMMAND ${CMAKE_COMMAND} -E make_directory
          ${OG_SYSTEM_ROOT}/system/boot
          ${OG_SYSTEM_ROOT}/system/libraries
          ${OG_SYSTEM_ROOT}/system/services
          ${OG_SYSTEM_ROOT}/system/config
          ${OG_SYSTEM_ROOT}/system/commands
          ${OG_SYSTEM_ROOT}/applications/gemabox/resources
          ${OG_SYSTEM_ROOT}/applications/hello
          ${OG_SYSTEM_ROOT}/commands
          ${OG_SYSTEM_ROOT}/etc
          ${OG_SYSTEM_ROOT}/users
          ${OG_SYSTEM_ROOT}/tmp
          ${OG_SYSTEM_ROOT}/var/log
          ${OG_SYSTEM_ROOT}/var/run
          ${OG_SYSTEM_ROOT}/var/cache
          ${OG_SYSTEM_ROOT}/var/spool
          ${OG_SYSTEM_ROOT}/dev
          ${OG_SYSTEM_ROOT}/proc
          ${OG_SYSTEM_ROOT}/volumes/boot
  COMMAND ${CMAKE_COMMAND} -E copy ${OG_GEMABOX_ELF}
          ${OG_SYSTEM_ROOT}/applications/gemabox/gemabox
  COMMAND ${CMAKE_COMMAND} -E copy ${OG_LDGEM_ELF}
          ${OG_SYSTEM_ROOT}/system/boot/ld-gem
  COMMAND ${CMAKE_COMMAND} -E copy ${OG_INIT_ELF}
          ${OG_SYSTEM_ROOT}/system/boot/init
  COMMAND ${CMAKE_COMMAND} -E copy ${OG_HELLO_ELF}
          ${OG_SYSTEM_ROOT}/applications/hello/hello
  COMMAND ${CMAKE_COMMAND} -E copy
          ${CMAKE_SOURCE_DIR}/packages/system-root/applications/hello/manifest.txt
          ${OG_SYSTEM_ROOT}/applications/hello/manifest.txt
  COMMAND ${CMAKE_COMMAND} -E copy ${OG_GEMAPI_SO}
          ${OG_SYSTEM_ROOT}/system/libraries/libgemapi.so.1
  COMMAND ${CMAKE_COMMAND} -E copy ${OG_KGEMA_SO}
          ${OG_SYSTEM_ROOT}/system/libraries/libkgema.so.1
  COMMAND ${CMAKE_COMMAND} -E copy
          ${CMAKE_SOURCE_DIR}/packages/system-root/system/config/version
          ${OG_SYSTEM_ROOT}/system/config/version
  COMMAND ${CMAKE_COMMAND} -E copy
          ${CMAKE_SOURCE_DIR}/packages/system-root/system/config/gsl.conf
          ${OG_SYSTEM_ROOT}/system/config/gsl.conf
  COMMAND ${CMAKE_COMMAND} -E copy
          ${CMAKE_SOURCE_DIR}/packages/system-root/applications/gemabox/manifest.txt
          ${OG_SYSTEM_ROOT}/applications/gemabox/manifest.txt
  COMMAND ${OG_PYTHON} ${CMAKE_SOURCE_DIR}/scripts/mkgslimg.py
          ${OG_SYSTEM_ROOT} ${OG_SYSTEM_IMAGE}
  COMMAND ${OG_PYTHON} ${CMAKE_SOURCE_DIR}/scripts/mkgslimg.py
          --verify ${OG_SYSTEM_IMAGE}
  DEPENDS
    ${OG_GEMABOX_ELF}
    ${OG_LDGEM_ELF}
    ${OG_INIT_ELF}
    ${OG_HELLO_ELF}
    ${OG_GEMAPI_SO}
    ${OG_KGEMA_SO}
    ${CMAKE_SOURCE_DIR}/packages/system-root/system/config/version
    ${CMAKE_SOURCE_DIR}/packages/system-root/system/config/gsl.conf
    ${CMAKE_SOURCE_DIR}/packages/system-root/applications/gemabox/manifest.txt
    ${CMAKE_SOURCE_DIR}/packages/system-root/applications/hello/manifest.txt
    ${CMAKE_SOURCE_DIR}/scripts/mkgslimg.py
  COMMENT "build gema system image")

add_custom_target(og_system_image ALL DEPENDS ${OG_SYSTEM_IMAGE})

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
    kernel/fs/fd_table.cpp
    kernel/fs/devfs.cpp
    kernel/fs/ramfs.cpp
    kernel/fs/procfs.cpp
    kernel/fs/partition.cpp
    kernel/fs/fat32.cpp
    kernel/fs/gslfs.cpp
  DEPS og_sched og_mem og_core og_inc_kernel)

og_add_module(og_sched
  SOURCES kernel/sched/scheduler.cpp
  DEPS og_events og_time og_mem og_core og_inc_kernel)

og_add_module(og_process
  SOURCES kernel/process/process.cpp kernel/process/elf.cpp
          kernel/process/loader.cpp
  DEPS og_fs og_sched og_mem og_core og_inc_kernel)

og_add_module(og_syscall
  SOURCES kernel/syscall/syscall.cpp kernel/syscall/io.cpp
          kernel/syscall/fs.cpp kernel/syscall/process.cpp
          kernel/syscall/memory.cpp kernel/syscall/helpers.cpp
  DEPS og_process og_sched og_events og_mem og_core og_inc_kernel)

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
  SOURCES
    kernel/core/init.cpp
    kernel/core/init/devices.cpp
    kernel/core/init/filesystem.cpp
    kernel/core/init/self_test.cpp
    kernel/core/init/user.cpp
  DEPS og_process og_sync og_platform og_syscall og_sched og_fs og_mem og_events og_time og_dma og_drivers og_inc_kernel)

og_add_module(og_boot
  SOURCES
    kernel/boot/uefi/efi_main.cpp
    kernel/boot/uefi/efi_console.cpp
    kernel/boot/uefi/efi_memmap.cpp
    kernel/boot/uefi/efi_image.cpp
    kernel/boot/uefi/efi_exit.cpp
  DEPS og_kernel og_inc_efi og_inc_kernel)

add_dependencies(og_kernel og_gemabox og_ld_gem og_init)

# Direct PE/COFF link. lld-link emits proper headers and .reloc.
set(OG_IMAGE_OBJECTS
  $<TARGET_OBJECTS:og_boot>
  $<TARGET_OBJECTS:og_kernel>
  $<TARGET_OBJECTS:og_drivers>
  $<TARGET_OBJECTS:og_fs>
  $<TARGET_OBJECTS:og_sched>
  $<TARGET_OBJECTS:og_process>
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
  DEPENDS og_boot og_kernel og_drivers og_fs og_sched og_process og_sync og_syscall og_platform og_time og_dma og_events og_mem og_core og_io og_arch og_lib_core
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

# GSL guard: applications only use libgemapi, and only libkgema has svc.
add_custom_target(og_gsl_check ALL
  COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/check-gsl.sh
  COMMENT "check GSL layer rules"
  VERBATIM)

add_dependencies(BOOTAA64EFI og_gsl_check)

add_custom_command(TARGET BOOTAA64EFI POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/esp/EFI/BOOT
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/BOOTAA64.EFI ${CMAKE_SOURCE_DIR}/esp/EFI/BOOT/BOOTAA64.EFI
  COMMENT "Copy BOOTAA64.EFI to esp/EFI/BOOT")
