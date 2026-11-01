#pragma once

// Deferred work run at the end of interrupt handling.

#include <cstdint>

#include "og/core/status.h"

namespace og::events {

using WorkFn = void (*)(void* context);

[[nodiscard]] Status WorkQueueInit() noexcept;
[[nodiscard]] Status ScheduleWork(WorkFn function, void* context) noexcept;

// Runs every queued item and returns how many ran.
[[nodiscard]] uint32_t RunPendingWork() noexcept;

[[nodiscard]] uint32_t PendingWork() noexcept;

}  // namespace og::events
