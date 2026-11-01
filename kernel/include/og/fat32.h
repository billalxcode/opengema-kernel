#pragma once

// FAT32 read-only driver.

#include "og/block_device.h"
#include "og/status.h"
#include "og/vfs.h"

namespace og::fs::fat32 {

// Mounts a FAT32 volume and returns its root directory node.
[[nodiscard]] Status Mount(const BlockDevice& device, Node** root_out) noexcept;

}  // namespace og::fs::fat32
