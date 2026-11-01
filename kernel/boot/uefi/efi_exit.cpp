#include "og/boot/uefi/efi_boot.h"

#include "og/core/bootinfo.h"
#include "og/core/log.h"

namespace og::boot::uefi {

Status TakeOver(EFI_HANDLE image, EFI_SYSTEM_TABLE* system_table, BootInfo* out) noexcept {
  if (system_table == nullptr) {
    return Status::kInvalidArg;
  }
  if (system_table->boot_services == nullptr) {
    return Status::kNoDevice;
  }
  if (system_table->boot_services->exit_boot_services == nullptr) {
    return Status::kUnsupported;
  }
  if (out == nullptr) {
    return Status::kInvalidArg;
  }

  // A key becomes stale if any boot service runs after GetMemoryMap.
  // Recapture and retry a few times before giving up.
  constexpr uint32_t kMaxAttempts = 4;
  for (uint32_t attempt = 0; attempt < kMaxAttempts; ++attempt) {
    Result<MemoryMapSnapshot> snapshot = CaptureMemoryMap(system_table, out);
    if (snapshot.is_err()) {
      LogHex(LogLevel::kError, "uefi: capture failed", static_cast<uint64_t>(snapshot.status()));
      return snapshot.status();
    }
    LogDec(LogLevel::kDebug, "uefi: map descriptors", snapshot.value().descriptor_count);
    const UINTN map_key = snapshot.value().map_key;
    const EFI_STATUS efi_status =
      system_table->boot_services->exit_boot_services(image, map_key);
    if (!EfiFailed(efi_status)) {
      return Status::kOk;
    }
    LogDec(LogLevel::kError, "uefi: exit attempt failed", attempt);
    LogHex(LogLevel::kError, "uefi: efi status", efi_status);
  }
  return Status::kIoError;
}

}  // namespace og::boot::uefi
