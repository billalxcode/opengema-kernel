#include "efi_boot.h"

#include "og/core/bootinfo.h"
#include "og/platform/memory_map.h"

namespace {

// 32 KiB covers QEMU virt with a wide margin. EDK2 typically reports
// fewer than 100 descriptors at 40 bytes each.
alignas(16) uint8_t g_map_buffer[32U * 1024U];

constexpr uint64_t kPageSize = 4096U;

[[nodiscard]] og::MemoryType MapMemoryType(uint32_t efi_type) noexcept {
  switch (efi_type) {
    case kEfiConventionalMemory:
      return og::MemoryType::kUsable;
    case kEfiAcpiReclaimMemory:
      return og::MemoryType::kAcpiReclaim;
    case kEfiAcpiMemoryNvs:
      return og::MemoryType::kAcpiNvs;
    case kEfiMemoryMappedIo:
    case kEfiMemoryMappedIoPortSpace:
      return og::MemoryType::kMmio;
    case kEfiRuntimeServicesCode:
    case kEfiRuntimeServicesData:
      return og::MemoryType::kRuntime;
    case kEfiBootServicesCode:
    case kEfiBootServicesData:
      return og::MemoryType::kBootServices;
    case kEfiLoaderCode:
    case kEfiLoaderData:
    case kEfiUnusableMemory:
      return og::MemoryType::kReserved;
    default:
      return og::MemoryType::kUnknown;
  }
}

}  // namespace

namespace og::boot::uefi {

Result<MemoryMapSnapshot> CaptureMemoryMap(
  EFI_SYSTEM_TABLE* system_table,
  BootInfo* out) noexcept {
  if (system_table == nullptr) {
    return Result<MemoryMapSnapshot>::Err(Status::kInvalidArg);
  }
  if (system_table->boot_services == nullptr) {
    return Result<MemoryMapSnapshot>::Err(Status::kNoDevice);
  }
  if (out == nullptr) {
    return Result<MemoryMapSnapshot>::Err(Status::kInvalidArg);
  }
  if (system_table->boot_services->get_memory_map == nullptr) {
    return Result<MemoryMapSnapshot>::Err(Status::kUnsupported);
  }

  UINTN map_size = sizeof(g_map_buffer);
  UINTN map_key = 0;
  UINTN descriptor_size = 0;
  uint32_t descriptor_version = 0;

  const EFI_STATUS efi_status = system_table->boot_services->get_memory_map(
    &map_size, g_map_buffer, &map_key, &descriptor_size, &descriptor_version);
  if (EfiFailed(efi_status)) {
    return Result<MemoryMapSnapshot>::Err(Status::kNoMem);
  }
  if (descriptor_size < sizeof(EFI_MEMORY_DESCRIPTOR)) {
    return Result<MemoryMapSnapshot>::Err(Status::kInvalidArg);
  }

  uint64_t count = 0;
  uint64_t ram_end = og::memmap::kRamBase;
  for (std::size_t offset = 0; offset + descriptor_size <= map_size; offset += descriptor_size) {
    if (count >= kMaxMemoryRanges) {
      break;
    }
    const auto* descriptor =
      reinterpret_cast<const EFI_MEMORY_DESCRIPTOR*>(g_map_buffer + offset);
    MemoryRange& range = out->memory[count];
    range.base = descriptor->physical_start;
    range.size = descriptor->number_of_pages * kPageSize;
    range.type = MapMemoryType(descriptor->type);
    range.reserved = 0;
    ++count;

    // Device space below RAM stays device. RAM span drives the identity map.
    if (range.type != og::MemoryType::kMmio) {
      if (range.base >= og::memmap::kRamBase) {
        const uint64_t end = range.base + range.size;
        if (end > ram_end) {
          ram_end = end;
        }
      }
    }
  }

  out->magic = kBootInfoMagic;
  out->version = kBootInfoVersion;
  out->size = sizeof(BootInfo);
  out->memory_count = count;
  out->ram_base = og::memmap::kRamBase;
  out->ram_size = ram_end > og::memmap::kRamBase ? (ram_end - og::memmap::kRamBase) : 0;

  MemoryMapSnapshot snapshot{};
  snapshot.map_key = map_key;
  snapshot.descriptor_size = descriptor_size;
  snapshot.descriptor_count = count;
  return Result<MemoryMapSnapshot>::Ok(snapshot);
}

}  // namespace og::boot::uefi
