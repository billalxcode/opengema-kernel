#pragma once

// Minimal UEFI headers for BOOTAA64.EFI.
// Only what efi_main.cpp needs. Not full EDK2.

#include <cstddef>
#include <cstdint>

using CHAR16 = wchar_t;
using EFI_HANDLE = void*;
using EFI_STATUS = uint64_t;
using UINTN = uint64_t;

constexpr EFI_STATUS kEfiSuccess = 0;

// EFI_TABLE_HEADER is exactly 24 bytes (spec, any arch).
struct EfiTableHeader {
  uint64_t signature;
  uint32_t revision;
  uint32_t header_size;
  uint32_t crc32;
  uint32_t reserved;
};

static_assert(sizeof(EfiTableHeader) == 24);

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
  void* reset;
  EFI_STATUS (*output_string)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* self, const CHAR16* str);
  void* test_string;
  void* query_mode;
  void* set_mode;
  void* set_attribute;
  void* clear_screen;
  void* set_cursor_position;
  void* enable_cursor;
  void* mode;
};

struct EFI_BOOT_SERVICES {
  void* raise_tpl;
  void* restore_tpl;
  void* allocate_pages;
  void* free_pages;
  EFI_STATUS (*get_memory_map)(
    UINTN* memory_map_size,
    void* memory_map,
    UINTN* map_key,
    UINTN* descriptor_size,
    uint32_t* descriptor_version);
  void* allocate_pool;
  void* free_pool;
  void* create_event;
  void* set_timer;
  void* wait_for_event;
  void* signal_event;
  void* close_event;
  void* check_event;
  void* install_protocol;
  void* reinstall_protocol;
  void* uninstall_protocol;
  EFI_STATUS (*exit_boot_services)(EFI_HANDLE image, UINTN map_key);
  void* reserved[8];
};

struct EFI_SYSTEM_TABLE {
  EfiTableHeader hdr;
  CHAR16* firmware_vendor;             // 0x18
  uint32_t firmware_revision;          // 0x20
  uint32_t reserved_align;
  EFI_HANDLE console_in_handle;        // 0x28
  void* con_in;                        // 0x30
  EFI_HANDLE console_out_handle;       // 0x38
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* con_out;  // 0x40
  void* stderr_handle;                 // 0x48
  void* std_err;                       // 0x50
  void* runtime_services;              // 0x58
  EFI_BOOT_SERVICES* boot_services;    // 0x60
  UINTN number_of_table_entries;       // 0x68
  void* configuration_table;           // 0x70
};

static_assert(sizeof(EfiTableHeader) == 24);
static_assert(offsetof(EFI_SYSTEM_TABLE, con_out) == 0x40);
static_assert(offsetof(EFI_SYSTEM_TABLE, boot_services) == 0x60);
