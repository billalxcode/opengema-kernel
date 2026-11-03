#pragma once

// Internal VFS state shared by the mount/path code and the file descriptor
// table. This header is private to kernel/fs and is not a public API.

#include "og/fs/vfs.h"

namespace og::fs {

// True once FsInit has initialized the mount table.
[[nodiscard]] bool IsReady() noexcept;

// Keeps the mount table from releasing a root while a VFS operation is
// using nodes returned by resolution. Nested guards are allowed.
class OperationGuard {
 public:
  OperationGuard() noexcept;
  ~OperationGuard() noexcept;

  OperationGuard(const OperationGuard&) = delete;
  OperationGuard& operator=(const OperationGuard&) = delete;
};

// Resolves a path and reports which mount entry served it. The caller must
// hold an OperationGuard and call TrackOpen once a descriptor owns the node.
[[nodiscard]] Result<Node*> ResolveForOpen(const char* path,
                                           uint32_t& mount_index) noexcept;

// Counts one open descriptor against a mount entry.
[[nodiscard]] Status TrackOpen(uint32_t mount_index) noexcept;

// Drops one open descriptor and releases a pending unmount when possible.
void UntrackOpen(uint32_t mount_index) noexcept;

}  // namespace og::fs
