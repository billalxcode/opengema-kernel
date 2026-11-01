#pragma once

// In-memory writable filesystem with directories. The kernel uses it for the
// fixed writable mounts (/etc, /users, /var, /tmp) until a persistent
// writable filesystem exists.

#include "og/core/status.h"
#include "og/fs/vfs.h"

namespace og::fs::ramfs {

// Creates the root directory and resets the node pool.
[[nodiscard]] Result<Node*> Create() noexcept;

[[nodiscard]] Result<Node*> CreateDir(Node* parent, const char* name) noexcept;

[[nodiscard]] Result<Node*> CreateFile(Node* parent, const char* name,
                                       const void* data, uint64_t size) noexcept;

}  // namespace og::fs::ramfs
