#pragma once

// Device filesystem: /dev/console, /dev/null, /dev/zero.

#include "og/core/status.h"
#include "og/fs/vfs.h"

namespace og::fs::devfs {

// Creates the root directory node for a devfs mount.
[[nodiscard]] Result<Node*> Create() noexcept;

}  // namespace og::fs::devfs
