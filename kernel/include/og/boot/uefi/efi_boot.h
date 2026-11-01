#pragma once

// Internal boot contract between the UEFI loader layers.
// Not part of the kernel public API.

#include "efi.h"
#include "og/core/status.h"

namespace og {

struct BootInfo;

namespace boot::uefi {

struct MemoryMapSnapshot {
  UINTN map_key;
  UINTN descriptor_size;
  UINTN descriptor_count;
};

// Fills BootInfo from the current UEFI memory map and returns the snapshot
// needed to call ExitBootServices. No boot service may be called between
// this capture and ExitBootServices.
[[nodiscard]] Result<MemoryMapSnapshot> CaptureMemoryMap(
  EFI_SYSTEM_TABLE* system_table,
  BootInfo* out) noexcept;

// Reads the loaded image protocol and the PE section table, then stores the
// image range and the executable section range for W^X.
[[nodiscard]] Status QueryImageSections(EFI_HANDLE image, EFI_SYSTEM_TABLE* system_table,
                                        BootInfo* out) noexcept;

// Captures the memory map and leaves boot services, retrying on stale keys.
[[nodiscard]] Status TakeOver(
  EFI_HANDLE image,
  EFI_SYSTEM_TABLE* system_table,
  BootInfo* out) noexcept;

}  // namespace boot::uefi
}  // namespace og
