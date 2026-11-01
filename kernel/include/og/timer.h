#pragma once

// Canonical owner of the kernel timer service. The ARM generic timer is
// a system register block, not a bus device, so the driver lives here
// instead of drivers/.

#include <cstdint>

#include "og/status.h"

namespace og::time {

// INTID 30 is the EL1 physical timer PPI on ARM.
constexpr uint32_t kTimerIrq = 30;

[[nodiscard]] Status TimerInit() noexcept;
[[nodiscard]] Status TimerStartPeriodic(uint32_t hz) noexcept;
[[nodiscard]] Status TimerStop() noexcept;

[[nodiscard]] uint64_t TimerTicks() noexcept;
[[nodiscard]] uint64_t TimerFrequency() noexcept;

}  // namespace og::time
