#pragma once

// Minimal UEFI definitions for BOOTAA64.EFI. Not full EDK2.
// Layouts are checked with static_assert against the UEFI spec offsets.

#include <cstddef>
#include <cstdint>

using CHAR16 = wchar_t;
using EFI_HANDLE = void*;
using EFI_STATUS = uint64_t;
using UINTN = uint64_t;

constexpr EFI_STATUS kEfiSuccess = 0;

[[nodiscard]] constexpr bool EfiFailed(EFI_STATUS status) noexcept {
  return (status >> 63U) != 0U;
}

// EFI_TABLE_HEADER is exactly 24 bytes on every architecture.
struct EfiTableHeader {
  uint64_t signature;
  uint32_t revision;
  uint32_t header_size;
  uint32_t crc32;
  uint32_t reserved;
};

static_assert(sizeof(EfiTableHeader) == 24);

// EFI_MEMORY_DESCRIPTOR, 40 bytes on 64-bit firmware.
struct EFI_MEMORY_DESCRIPTOR {
  uint32_t type;
  uint32_t pad;
  uint64_t physical_start;
  uint64_t virtual_start;
  uint64_t number_of_pages;
  uint64_t attribute;
};

static_assert(sizeof(EFI_MEMORY_DESCRIPTOR) == 40);

constexpr uint32_t kEfiLoaderCode = 1;
constexpr uint32_t kEfiLoaderData = 2;
constexpr uint32_t kEfiBootServicesCode = 3;
constexpr uint32_t kEfiBootServicesData = 4;
constexpr uint32_t kEfiRuntimeServicesCode = 5;
constexpr uint32_t kEfiRuntimeServicesData = 6;
constexpr uint32_t kEfiConventionalMemory = 7;
constexpr uint32_t kEfiUnusableMemory = 8;
constexpr uint32_t kEfiAcpiReclaimMemory = 9;
constexpr uint32_t kEfiAcpiMemoryNvs = 10;
constexpr uint32_t kEfiMemoryMappedIo = 11;
constexpr uint32_t kEfiMemoryMappedIoPortSpace = 12;

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
  void* reset;
  EFI_STATUS (*output_string)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* self, const CHAR16* text);
  void* test_string;
  void* query_mode;
  void* set_mode;
  void* set_attribute;
  void* clear_screen;
  void* set_cursor_position;
  void* enable_cursor;
  void* mode;
};

// Field order follows the UEFI spec. EFI_BOOT_SERVICES starts with a table
// header, which offsets every service pointer by 24 bytes.
// ExitBootServices is index 26 after the header.
struct EFI_BOOT_SERVICES {
  EfiTableHeader hdr;                 // 0x00
  void* raise_tpl;                    // 0x18
  void* restore_tpl;                  // 0x20
  void* allocate_pages;               // 0x28
  void* free_pages;                   // 0x30
  EFI_STATUS (*get_memory_map)(       // 0x38
    UINTN* memory_map_size,
    void* memory_map,
    UINTN* map_key,
    UINTN* descriptor_size,
    uint32_t* descriptor_version);
  void* allocate_pool;                // 0x40
  void* free_pool;                    // 0x48
  void* create_event;                 // 0x50
  void* set_timer;                    // 0x58
  void* wait_for_event;               // 0x60
  void* signal_event;                 // 0x68
  void* close_event;                  // 0x70
  void* check_event;                  // 0x78
  void* install_protocol_interface;   // 0x80
  void* reinstall_protocol_interface; // 0x88
  void* uninstall_protocol_interface; // 0x90
  void* handle_protocol;              // 0x98
  void* reserved;                     // 0xa0
  void* register_protocol_notify;     // 0xa8
  void* locate_handle;                // 0xb0
  void* locate_device_path;           // 0xb8
  void* install_configuration_table;  // 0xc0
  void* load_image;                   // 0xc8
  void* start_image;                  // 0xd0
  void* exit;                         // 0xd8
  void* unload_image;                 // 0xe0
  EFI_STATUS (*exit_boot_services)(EFI_HANDLE image, UINTN map_key);  // 0xe8
  void* get_next_monotonic_count;     // 0xf0
  void* stall;                        // 0xf8
  void* set_watchdog_timer;           // 0x100
};

struct EFI_SYSTEM_TABLE {
  EfiTableHeader hdr;                   // 0x00
  CHAR16* firmware_vendor;              // 0x18
  uint32_t firmware_revision;           // 0x20
  uint32_t reserved_align;              // 0x24
  EFI_HANDLE console_in_handle;         // 0x28
  void* con_in;                         // 0x30
  EFI_HANDLE console_out_handle;        // 0x38
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* con_out;  // 0x40
  void* stderr_handle;                  // 0x48
  void* std_err;                        // 0x50
  void* runtime_services;               // 0x58
  EFI_BOOT_SERVICES* boot_services;     // 0x60
  UINTN number_of_table_entries;        // 0x68
  void* configuration_table;            // 0x70
};

static_assert(offsetof(EFI_SYSTEM_TABLE, con_out) == 0x40);
static_assert(offsetof(EFI_SYSTEM_TABLE, boot_services) == 0x60);
static_assert(offsetof(EFI_BOOT_SERVICES, get_memory_map) == 0x38);
static_assert(offsetof(EFI_BOOT_SERVICES, exit_boot_services) == 0xe8);
