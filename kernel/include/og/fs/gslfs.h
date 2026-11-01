#pragma once

// Read-only GSLFS driver. Mounts a system image from a block device and
// exposes its nodes through the VFS.

#include <cstdint>

#include "og/core/status.h"
#include "og/fs/block_device.h"
#include "og/fs/vfs.h"

namespace og::fs::gslfs {

constexpr uint8_t kMagic[8] = {'G', 'S', 'L', 'F', 'S', '1', '\0', '\0'};
constexpr uint32_t kVersion = 1;
constexpr uint32_t kBlockSize = 4096;
constexpr uint32_t kMaxNodes = 128;

// Validates the superblock, node table, names, and file checksums.
[[nodiscard]] Result<Node*> Mount(const BlockDevice& device) noexcept;

[[nodiscard]] uint32_t NodeCount() noexcept;

}  // namespace og::fs::gslfs
