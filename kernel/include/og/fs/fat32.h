#pragma once

// FAT32 read-only driver.

#include "og/core/status.h"
#include "og/fs/block_device.h"
#include "og/fs/vfs.h"

namespace og::fs::fat32 {

// Mounts a FAT32 volume and returns its root directory node.
[[nodiscard]] Status Mount(const BlockDevice& device, Node** root_out) noexcept;

}  // namespace og::fs::fat32
