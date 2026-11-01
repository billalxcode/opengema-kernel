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

struct GptHeader {
  uint64_t entries_lba;
  uint32_t count;
  uint32_t entry_size;
};

[[nodiscard]] Result<GptHeader> ReadGptHeader(const BlockDevice& device) noexcept {
  uint8_t sector[kSectorSize];
  const Status status =
    device.read(device.context, 1, std::span<uint8_t>(sector, kSectorSize));
  if (IsErr(status)) {
    return Result<GptHeader>::Err(status);
  }
  if (!mem::Equal(sector, "EFI PART", 8)) {
    return Result<GptHeader>::Err(Status::kNoDevice);
  }

  GptHeader header{};
  header.entries_lba = endian::ReadLe64(sector + 72);
  header.count = endian::ReadLe32(sector + 80);
  header.entry_size = endian::ReadLe32(sector + 84);
  if (header.count == 0U) {
    return Result<GptHeader>::Err(Status::kInvalidArg);
  }
  if (header.entry_size < kMinEntrySize) {
    return Result<GptHeader>::Err(Status::kInvalidArg);
  }
  if (header.entries_lba == 0U) {
    return Result<GptHeader>::Err(Status::kInvalidArg);
  }
  if (header.count > kMaxEntries) {
    header.count = static_cast<uint32_t>(kMaxEntries);
  }
  return Result<GptHeader>::Ok(header);
}

[[nodiscard]] bool MatchesType(const uint8_t* entry,
                               const PartitionFilter& filter) noexcept {
  return filter.type_guid == nullptr ||
         mem::Equal(entry + kTypeGuidOffset, filter.type_guid, 16);
}

// Fills the partition from one entry. Returns false for empty or
// out-of-disk ranges, which GPT tables may legally contain.
[[nodiscard]] bool ParsePartition(const uint8_t* entry,
                                  const BlockDevice& device,
                                  Partition& partition) noexcept {
  const uint64_t first_lba = endian::ReadLe64(entry + kStartLbaOffset);
  const uint64_t last_lba = endian::ReadLe64(entry + kLastLbaOffset);
  if (first_lba == 0U || last_lba < first_lba) {
    return false;
  }
  partition.start_sector = first_lba;
  partition.sector_count = (last_lba - first_lba) + 1U;
  if (partition.start_sector > device.sector_count) {
    return false;
  }
  return partition.sector_count <=
         (device.sector_count - partition.start_sector);
}

[[nodiscard]] Status CheckMagic(const BlockDevice& device,
                                const Partition& partition,
                                const PartitionFilter& filter,
                                bool& matches) noexcept {
  matches = true;
  if (filter.magic == nullptr) {
    return Status::kOk;
  }
  if (filter.magic_size == 0U || filter.magic_size > kSectorSize) {
    return Status::kInvalidArg;
  }
  uint8_t sector[kSectorSize];
  const Status status = device.read(
    device.context, device.sector_offset + partition.start_sector,
    std::span<uint8_t>(sector, kSectorSize));
  if (IsErr(status)) {
    return status;
  }
  matches = mem::Equal(sector, filter.magic, filter.magic_size);
  return Status::kOk;
}

[[nodiscard]] Result<Partition> FindFiltered(const BlockDevice& device,
                                             const PartitionFilter& filter) noexcept {
  if (device.read == nullptr) {
    return Result<Partition>::Err(Status::kInvalidArg);
  }
  if (device.sector_count < 2U) {
    return Result<Partition>::Err(Status::kInvalidArg);
  }

  const Result<GptHeader> header = ReadGptHeader(device);
  if (header.is_err()) {
    return Result<Partition>::Err(header.status());
  }

  uint8_t buffer[kSectorSize];
  uint64_t cached_lba = ~0ULL;

  for (uint32_t index = 0; index < header.value().count; ++index) {
    const uint64_t entry_offset =
      static_cast<uint64_t>(index) * header.value().entry_size;
    const uint64_t lba = header.value().entries_lba +
                         (entry_offset / kSectorSize);
    const uint32_t within = static_cast<uint32_t>(entry_offset % kSectorSize);
    if ((within + kMinEntrySize) > kSectorSize) {
      continue;
    }

    if (lba != cached_lba) {
      const Status status =
        device.read(device.context, lba, std::span<uint8_t>(buffer, kSectorSize));
      if (IsErr(status)) {
        return Result<Partition>::Err(status);
      }
      cached_lba = lba;
    }

    const uint8_t* entry = buffer + within;
    if (!MatchesType(entry, filter)) {
      continue;
    }

    Partition partition{};
    if (!ParsePartition(entry, device, partition)) {
      continue;
    }

    bool matches = false;
    const Status status = CheckMagic(device, partition, filter, matches);
    if (IsErr(status)) {
      return Result<Partition>::Err(status);
    }
    if (matches) {
      return Result<Partition>::Ok(partition);
    }
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
