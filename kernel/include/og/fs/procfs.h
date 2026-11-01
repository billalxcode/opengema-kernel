#pragma once

// Process information filesystem: /proc/uptime, /proc/threads,
// /proc/acpi, and /proc/processes.

#include <cstddef>

#include "og/core/status.h"
#include "og/fs/vfs.h"

namespace og::fs::procfs {

using ThreadCountFn = uint32_t (*)() noexcept;
using ProcessTextFn = Status (*)(char* buffer, std::size_t capacity,
                                 std::size_t& length) noexcept;

[[nodiscard]] Result<Node*> Create(ThreadCountFn thread_count,
                                  ProcessTextFn process_text) noexcept;

}  // namespace og::fs::procfs
