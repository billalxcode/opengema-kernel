#pragma once

// GPT partition lookup. Finds the EFI System Partition so a filesystem can
// be mounted with the right sector offset.

#include <cstdint>

#include "og/core/status.h"
#include "og/fs/block_device.h"

namespace og::fs {

struct Partition {
  uint64_t start_sector;
  uint64_t sector_count;
};

[[nodiscard]] Result<Partition> FindEspPartition(const BlockDevice& device) noexcept;

}  // namespace og::fs
