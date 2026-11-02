#pragma once

// Block device interface used by filesystems. Drivers adapt their own
// read function to this shape, so fs never depends on a driver header.

#include <cstdint>
#include <span>

#include "og/core/status.h"

namespace og::fs {

struct BlockDevice {
  using ReadFn = Status (*)(void* context, uint64_t sector, std::span<uint8_t> buffer);
  using WriteFn = Status (*)(void* context, uint64_t sector,
                             std::span<const uint8_t> buffer);

  ReadFn read;
  void* context;
  uint64_t sector_count;
  // Added to every sector number. Non-zero when the filesystem starts at a
  // partition offset instead of the whole disk.
  uint64_t sector_offset;
  // Optional. Null for read-only volumes.
  WriteFn write = nullptr;
};

}  // namespace og::fs
