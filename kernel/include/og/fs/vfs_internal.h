#pragma once

// Internal VFS state shared by the mount/path code and the file descriptor
// table. This header is private to kernel/fs and is not a public API.

namespace og::fs {

// True once FsInit has initialized the mount table.
[[nodiscard]] bool IsReady() noexcept;

}  // namespace og::fs
