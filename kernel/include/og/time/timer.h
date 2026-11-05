#pragma once

// Canonical owner of the kernel timer service. The ARM generic timer is
// a system register block, not a bus device, so the driver lives here
// instead of drivers/.

#include <cstdint>

#include "og/core/status.h"

namespace og::time {

// INTID 30 is the EL1 physical timer PPI on ARM.
constexpr uint32_t kTimerIrq = 30;

using TickCallback = void (*)(void* context);

struct Timeout;

using TimeoutCallback = void (*)(void* context);

[[nodiscard]] Status TimerInit() noexcept;
[[nodiscard]] Status TimerStartPeriodic(uint32_t hz) noexcept;
[[nodiscard]] Status TimerStop() noexcept;

// Called from IRQ context after the tick counter is incremented.
void TimerSetTickCallback(TickCallback callback, void* context) noexcept;

[[nodiscard]] uint64_t TimerTicks() noexcept;
[[nodiscard]] uint64_t TimerFrequency() noexcept;

// Clocksource helpers. Safe before TimerInit; they read CNTFRQ directly.
[[nodiscard]] uint64_t TimeNowNs() noexcept;
[[nodiscard]] uint64_t TimeNowMs() noexcept;

// One shot timeout. The callback runs in IRQ context, so it must be short
// and must not sleep.
[[nodiscard]] Result<Timeout*> ScheduleAfter(uint64_t milliseconds,
                                            TimeoutCallback callback,
                                            void* context) noexcept;
[[nodiscard]] Status CancelTimeout(Timeout* timeout) noexcept;

// Wall clock source, normally the PL031 RTC. The kernel registers it during
// boot so syscalls do not depend on the driver directly.
using WallClockReadFn = Result<uint64_t> (*)(void* context) noexcept;
using WallClockWriteFn = Status (*)(void* context, uint64_t seconds) noexcept;
void SetWallClock(WallClockReadFn read, WallClockWriteFn write,
                  void* context) noexcept;
[[nodiscard]] Result<uint64_t> WallTimeSeconds() noexcept;
[[nodiscard]] Status SetWallTimeSeconds(uint64_t seconds) noexcept;

// Timezone offset east of UTC, in minutes. Valid range -840..840.
[[nodiscard]] Status SetTimeZoneMinutes(int32_t minutes) noexcept;
[[nodiscard]] int32_t TimeZoneMinutes() noexcept;

// Wall time plus the timezone offset.
[[nodiscard]] Result<uint64_t> LocalTimeSeconds() noexcept;

}  // namespace og::time
