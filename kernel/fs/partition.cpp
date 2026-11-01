#include "og/fs/partition.h"

#include "endian.h"
#include "memory.h"

namespace og::fs {

namespace {

constexpr uint32_t kSectorSize = 512;
constexpr uint64_t kMaxEntries = 128;
constexpr uint32_t kMinEntrySize = 128;
constexpr uint32_t kTypeGuidOffset = 0;
constexpr uint32_t kStartLbaOffset = 32;
constexpr uint32_t kLastLbaOffset = 40;

// EFI System Partition type GUID, mixed endian as stored on disk.
constexpr uint8_t kEspTypeGuid[16] = {0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11,
                                      0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B};

struct PartitionFilter {
  const uint8_t* type_guid;
  const uint8_t* magic;
  uint32_t magic_size;
};

[[nodiscard]] Result<Partition> FindFiltered(const BlockDevice& device,
                                             const PartitionFilter& filter) noexcept {
  if (device.read == nullptr) {
    return Result<Partition>::Err(Status::kInvalidArg);
  }
  if (device.sector_count < 2U) {
    return Result<Partition>::Err(Status::kInvalidArg);
  }

  uint8_t sector[kSectorSize];
  Status status = device.read(device.context, 1, std::span<uint8_t>(sector, kSectorSize));
  if (IsErr(status)) {
    return Result<Partition>::Err(status);
  }
  if (!mem::Equal(sector, "EFI PART", 8)) {
    return Result<Partition>::Err(Status::kNoDevice);
  }

  const uint64_t entries_lba = endian::ReadLe64(sector + 72);
  uint32_t count = endian::ReadLe32(sector + 80);
  const uint32_t entry_size = endian::ReadLe32(sector + 84);
  if (count == 0U) {
    return Result<Partition>::Err(Status::kInvalidArg);
  }
  if (entry_size < kMinEntrySize) {
    return Result<Partition>::Err(Status::kInvalidArg);
  }
  if (count > kMaxEntries) {
    count = static_cast<uint32_t>(kMaxEntries);
  }
  if (entries_lba == 0) {
    return Result<Partition>::Err(Status::kInvalidArg);
  }

  uint8_t buffer[kSectorSize];
  uint8_t magic_sector[kSectorSize];
  uint64_t cached_lba = ~0ULL;

  for (uint32_t index = 0; index < count; ++index) {
    const uint64_t entry_offset = static_cast<uint64_t>(index) * entry_size;
    const uint64_t lba = entries_lba + (entry_offset / kSectorSize);
    const uint32_t within = static_cast<uint32_t>(entry_offset % kSectorSize);
    if ((within + kMinEntrySize) > kSectorSize) {
      continue;
    }

    if (lba != cached_lba) {
      status = device.read(device.context, lba, std::span<uint8_t>(buffer, kSectorSize));
      if (IsErr(status)) {
        return Result<Partition>::Err(status);
      }
      cached_lba = lba;
    }

    const uint8_t* entry = buffer + within;
    if (filter.type_guid != nullptr &&
        !mem::Equal(entry + kTypeGuidOffset, filter.type_guid, 16)) {
      continue;
    }

    Partition partition{};
    const uint64_t first_lba = endian::ReadLe64(entry + kStartLbaOffset);
    const uint64_t last_lba = endian::ReadLe64(entry + kLastLbaOffset);
    if (first_lba == 0U || last_lba < first_lba) {
      continue;
    }
    partition.start_sector = first_lba;
    partition.sector_count = (last_lba - first_lba) + 1U;
    if (partition.start_sector > device.sector_count ||
        partition.sector_count > (device.sector_count - partition.start_sector)) {
      continue;
    }

    if (filter.magic != nullptr) {
      if (filter.magic_size == 0U || filter.magic_size > kSectorSize) {
        return Result<Partition>::Err(Status::kInvalidArg);
      }
      status = device.read(
        device.context, device.sector_offset + partition.start_sector,
        std::span<uint8_t>(magic_sector, kSectorSize));
      if (IsErr(status)) {
        return Result<Partition>::Err(status);
      }
      if (!mem::Equal(magic_sector, filter.magic, filter.magic_size)) {
        continue;
      }
    }
    return Result<Partition>::Ok(partition);
  }

  return Result<Partition>::Err(Status::kNotFound);
}

}  // namespace

Result<Partition> FindEspPartition(const BlockDevice& device) noexcept {
  return FindFiltered(device, PartitionFilter{kEspTypeGuid, nullptr, 0});
}

Result<Partition> FindPartitionByMagic(const BlockDevice& device,
                                       const uint8_t* magic,
                                       uint32_t magic_size) noexcept {
  if (magic == nullptr || magic_size == 0U) {
    return Result<Partition>::Err(Status::kInvalidArg);
  }
  return FindFiltered(device, PartitionFilter{nullptr, magic, magic_size});
}

}  // namespace og::fs
