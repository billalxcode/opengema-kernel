#pragma once

// In-memory filesystem with fixed capacity. Files can be created by the
// kernel and written through the VFS write path.

#include "og/status.h"
#include "og/vfs.h"

namespace og::fs::ramfs {

// Creates the root directory. Only one ramfs instance exists.
[[nodiscard]] Result<Node*> Create() noexcept;

// Creates a file with initial content. Content is copied.
[[nodiscard]] Result<Node*> CreateFile(Node* root, const char* name, const void* data,
                                       uint64_t size) noexcept;

}  // namespace og::fs::ramfs
