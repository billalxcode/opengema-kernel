#pragma once

// Process information filesystem: /proc/uptime and /proc/threads.

#include "og/core/status.h"
#include "og/fs/vfs.h"

namespace og::fs::procfs {

using ThreadCountFn = uint32_t (*)() noexcept;

[[nodiscard]] Result<Node*> Create(ThreadCountFn thread_count) noexcept;

}  // namespace og::fs::procfs
