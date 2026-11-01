#include "efi_boot.h"

#include "og/bootinfo.h"

namespace {

constexpr EFI_GUID kLoadedImageGuid{0x5B1B31A1U, 0x9562U, 0x11D2U,
                                    {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}};

constexpr uint32_t kPeSignature = 0x00004550U;
constexpr uint32_t kSectionExecute = 0x20000000U;
constexpr uint32_t kMaxSections = 32;
constexpr uint16_t kSectionHeaderSize = 40;

[[nodiscard]] uint16_t Read16(const uint8_t* data) noexcept {
  return static_cast<uint16_t>(static_cast<uint16_t>(data[0]) |
                               (static_cast<uint16_t>(data[1]) << 8U));
}

[[nodiscard]] uint32_t Read32(const uint8_t* data) noexcept {
  return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8U) |
         (static_cast<uint32_t>(data[2]) << 16U) | (static_cast<uint32_t>(data[3]) << 24U);
}

}  // namespace

namespace og::boot::uefi {

Status QueryImageSections(EFI_HANDLE image, EFI_SYSTEM_TABLE* system_table,
                          BootInfo* out) noexcept {
  if (system_table == nullptr) {
    return Status::kInvalidArg;
  }
  if (system_table->boot_services == nullptr) {
    return Status::kNoDevice;
  }
  if (system_table->boot_services->handle_protocol == nullptr) {
    return Status::kUnsupported;
  }
  if (out == nullptr) {
    return Status::kInvalidArg;
  }

  EFI_GUID guid = kLoadedImageGuid;
  void* interface = nullptr;
  const EFI_STATUS status =
    system_table->boot_services->handle_protocol(image, &guid, &interface);
  if (EfiFailed(status) || interface == nullptr) {
    return Status::kNoDevice;
  }

  auto* loaded = static_cast<EFI_LOADED_IMAGE_PROTOCOL*>(interface);
  if (loaded->image_base == nullptr || loaded->image_size == 0) {
    return Status::kInvalidArg;
  }

  const auto* base = static_cast<const uint8_t*>(loaded->image_base);
  if (base[0] != 'M' || base[1] != 'Z') {
    return Status::kNoDevice;
  }
  const uint32_t pe_offset = Read32(base + 0x3C);
  if (Read32(base + pe_offset) != kPeSignature) {
    return Status::kNoDevice;
  }

  const uint16_t section_count = Read16(base + pe_offset + 6);
  const uint16_t optional_size = Read16(base + pe_offset + 20);
  if (section_count == 0 || section_count > kMaxSections) {
    return Status::kInvalidArg;
  }

  const uint8_t* section = base + pe_offset + 24 + optional_size;
  uint64_t text_start = 0;
  uint64_t text_end = 0;
  for (uint16_t index = 0; index < section_count; ++index) {
    const uint32_t virtual_size = Read32(section + 8);
    const uint32_t virtual_address = Read32(section + 12);
    const uint32_t raw_size = Read32(section + 16);
    const uint32_t characteristics = Read32(section + 36);

    if ((characteristics & kSectionExecute) != 0U) {
      const uint64_t size = (virtual_size != 0U) ? virtual_size : raw_size;
      const uint64_t start = reinterpret_cast<uint64_t>(base) + virtual_address;
      if (text_start == 0 || start < text_start) {
        text_start = start;
      }
      if ((start + size) > text_end) {
        text_end = start + size;
      }
    }
    section += kSectionHeaderSize;
  }

  out->image_base = reinterpret_cast<uint64_t>(base);
  out->image_size = loaded->image_size;
  out->text_base = text_start;
  out->text_size = text_end > text_start ? (text_end - text_start) : 0;
  return Status::kOk;
}

}  // namespace og::boot::uefi
