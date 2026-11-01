#include "og/fs/partition.h"

#include "memory.h"

namespace og::fs {

namespace {

constexpr uint32_t kSectorSize = 512;
constexpr uint64_t kMaxEntries = 128;
constexpr uint32_t kMinEntrySize = 128;
constexpr uint32_t kTypeGuidOffset = 0;
constexpr uint32_t kStartLbaOffset = 32;
constexpr uint32_t kSectorCountOffset = 40;

// EFI System Partition type GUID, mixed endian as stored on disk.
constexpr uint8_t kEspTypeGuid[16] = {0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11,
                                      0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B};

[[nodiscard]] uint32_t Read32(const uint8_t* data) noexcept {
  return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8U) |
         (static_cast<uint32_t>(data[2]) << 16U) | (static_cast<uint32_t>(data[3]) << 24U);
}

[[nodiscard]] uint64_t Read64(const uint8_t* data) noexcept {
  uint64_t value = 0;
  for (uint32_t index = 0; index < 8U; ++index) {
    value |= static_cast<uint64_t>(data[index]) << (index * 8U);
  }
  return value;
}

}  // namespace

Result<Partition> FindEspPartition(const BlockDevice& device) noexcept {
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

  const uint64_t entries_lba = Read64(sector + 72);
  uint32_t count = Read32(sector + 80);
  const uint32_t entry_size = Read32(sector + 84);
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
    if (!mem::Equal(entry + kTypeGuidOffset, kEspTypeGuid, sizeof(kEspTypeGuid))) {
      continue;
    }

    Partition partition{};
    partition.start_sector = Read64(entry + kStartLbaOffset);
    partition.sector_count = Read64(entry + kSectorCountOffset);
    if (partition.start_sector == 0 || partition.sector_count == 0) {
      return Result<Partition>::Err(Status::kInvalidArg);
    }
    return Result<Partition>::Ok(partition);
  }

  return Result<Partition>::Err(Status::kNotFound);
}

}  // namespace og::fs
