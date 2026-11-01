#pragma once

// GPT partition lookup. Finds the EFI System Partition or a partition whose
// first sector starts with a known filesystem magic.

#include <cstdint>

#include "og/core/status.h"
#include "og/fs/block_device.h"

namespace og::fs {

struct Partition {
  uint64_t start_sector;
  uint64_t sector_count;
};

[[nodiscard]] Result<Partition> FindEspPartition(const BlockDevice& device) noexcept;

[[nodiscard]] Result<Partition> FindPartitionByMagic(const BlockDevice& device,
                                                     const uint8_t* magic,
                                                     uint32_t magic_size) noexcept;

}  // namespace og::fs
