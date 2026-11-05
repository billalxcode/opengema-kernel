#include "og/time/timer.h"

#include "og/arch/barriers.h"
#include "og/arch/registers.h"
#include "og/arch/spinlock.h"
#include "og/events/irq.h"

namespace og::time {

namespace {

constexpr uint32_t kTimeoutSlots = 32;
constexpr uint32_t kTimeoutBatch = 8;
constexpr uint64_t kNanosecondsPerSecond = 1000000000ULL;
constexpr uint64_t kNanosecondsPerMillisecond = 1000000ULL;

struct TimeoutSlot {
  bool used;
  uint64_t deadline_ns;
  TimeoutCallback callback;
  void* context;
};

TimeoutSlot g_timeouts[kTimeoutSlots];
arch::aarch64::IrqSpinlock g_timeout_lock;

uint64_t g_frequency = 0;
uint64_t g_period = 0;
uint64_t g_ticks = 0;
bool g_ready = false;
TickCallback g_tick_callback = nullptr;
void* g_tick_context = nullptr;
WallClockReadFn g_wall_read = nullptr;
WallClockWriteFn g_wall_write = nullptr;
void* g_wall_context = nullptr;
int32_t g_timezone_minutes = 0;

constexpr uint64_t kCntpCtlEnable = 1ULL << 0U;
constexpr uint64_t kCntpCtlMask = 1ULL << 1U;

[[nodiscard]] uint64_t Frequency() noexcept {
  if (g_frequency != 0) {
    return g_frequency;
  }
  return arch::aarch64::ReadCntfrqEl0();
}

void ProcessTimeouts() noexcept {
  TimeoutCallback callbacks[kTimeoutBatch];
  void* contexts[kTimeoutBatch];
  uint32_t count = 0;

  const uint64_t now = TimeNowNs();
  {
    arch::aarch64::LockGuard<arch::aarch64::IrqSpinlock> guard(g_timeout_lock);
    for (uint32_t index = 0; index < kTimeoutSlots && count < kTimeoutBatch; ++index) {
      if (!g_timeouts[index].used) {
        continue;
      }
      if (g_timeouts[index].deadline_ns > now) {
        continue;
      }
      callbacks[count] = g_timeouts[index].callback;
      contexts[count] = g_timeouts[index].context;
      g_timeouts[index].used = false;
      ++count;
    }
  }

  for (uint32_t index = 0; index < count; ++index) {
    callbacks[index](contexts[index]);
  }
}

[[nodiscard]] Status TimerHandler(void* /*context*/, uint32_t /*irq*/) noexcept {
  if (g_period != 0) {
    arch::aarch64::WriteCntpTvalEl0(g_period);
  }
  (void)__atomic_add_fetch(&g_ticks, 1U, __ATOMIC_RELAXED);
  ProcessTimeouts();
  if (g_tick_callback != nullptr) {
    g_tick_callback(g_tick_context);
  }
  return Status::kOk;
}

}  // namespace

uint64_t TimeNowNs() noexcept {
  const uint64_t frequency = Frequency();
  if (frequency == 0) {
    return 0;
  }
  const uint64_t ticks = arch::aarch64::ReadCntpctEl0();
  return ((ticks / frequency) * kNanosecondsPerSecond) +
         (((ticks % frequency) * kNanosecondsPerSecond) / frequency);
}

uint64_t TimeNowMs() noexcept {
  return TimeNowNs() / kNanosecondsPerMillisecond;
}

Result<Timeout*> ScheduleAfter(uint64_t milliseconds, TimeoutCallback callback,
                               void* context) noexcept {
  if (callback == nullptr) {
    return Result<Timeout*>::Err(Status::kInvalidArg);
  }
  const uint64_t deadline = TimeNowNs() + (milliseconds * kNanosecondsPerMillisecond);

  arch::aarch64::LockGuard<arch::aarch64::IrqSpinlock> guard(g_timeout_lock);
  for (uint32_t index = 0; index < kTimeoutSlots; ++index) {
    if (g_timeouts[index].used) {
      continue;
    }
    g_timeouts[index].used = true;
    g_timeouts[index].deadline_ns = deadline;
    g_timeouts[index].callback = callback;
    g_timeouts[index].context = context;
    return Result<Timeout*>::Ok(reinterpret_cast<Timeout*>(&g_timeouts[index]));
  }
  return Result<Timeout*>::Err(Status::kNoMem);
}

Status CancelTimeout(Timeout* timeout) noexcept {
  if (timeout == nullptr) {
    return Status::kInvalidArg;
  }
  auto* slot = reinterpret_cast<TimeoutSlot*>(timeout);
  arch::aarch64::LockGuard<arch::aarch64::IrqSpinlock> guard(g_timeout_lock);
  if (!slot->used) {
    return Status::kNotFound;
  }
  slot->used = false;
  return Status::kOk;
}

Status TimerInit() noexcept {
  if (g_ready) {
    return Status::kOk;
  }
  const uint64_t frequency = arch::aarch64::ReadCntfrqEl0();
  if (frequency == 0) {
    return Status::kNoDevice;
  }
  g_frequency = frequency;

  const Status status = events::RegisterIrq(kTimerIrq, &TimerHandler, nullptr);
  if (IsErr(status)) {
    return status;
  }

  arch::aarch64::WriteCntpCtlEl0(kCntpCtlMask);
  __atomic_store_n(&g_ticks, 0U, __ATOMIC_RELAXED);
  g_ready = true;
  return Status::kOk;
}

Status TimerStartPeriodic(uint32_t hz) noexcept {
  if (!g_ready) {
    return Status::kNotInit;
  }
  if (hz == 0) {
    return Status::kInvalidArg;
  }
  const uint64_t period = g_frequency / hz;
  if (period == 0) {
    return Status::kInvalidArg;
  }

  g_period = period;
  arch::aarch64::WriteCntpTvalEl0(period);
  arch::aarch64::WriteCntpCtlEl0(kCntpCtlEnable);
  arch::aarch64::InstructionSync();
  return Status::kOk;
}

Status TimerStop() noexcept {
  if (!g_ready) {
    return Status::kNotInit;
  }
  arch::aarch64::WriteCntpCtlEl0(kCntpCtlMask);
  g_period = 0;
  return Status::kOk;
}

uint64_t TimerTicks() noexcept {
  return __atomic_load_n(&g_ticks, __ATOMIC_ACQUIRE);
}

void TimerSetTickCallback(TickCallback callback, void* context) noexcept {
  g_tick_callback = callback;
  g_tick_context = context;
}

uint64_t TimerFrequency() noexcept {
  return g_frequency;
}

void SetWallClock(WallClockReadFn read, WallClockWriteFn write,
                  void* context) noexcept {
  g_wall_read = read;
  g_wall_write = write;
  g_wall_context = context;
}

Result<uint64_t> WallTimeSeconds() noexcept {
  if (g_wall_read == nullptr) {
    return Result<uint64_t>::Err(Status::kNotInit);
  }
  return g_wall_read(g_wall_context);
}

Status SetWallTimeSeconds(uint64_t seconds) noexcept {
  if (g_wall_write == nullptr) {
    return Status::kNotInit;
  }
  return g_wall_write(g_wall_context, seconds);
}

Status SetTimeZoneMinutes(int32_t minutes) noexcept {
  constexpr int32_t kMaxOffsetMinutes = 14 * 60;
  if (minutes < -kMaxOffsetMinutes || minutes > kMaxOffsetMinutes) {
    return Status::kInvalidArg;
  }
  __atomic_store_n(&g_timezone_minutes, minutes, __ATOMIC_RELEASE);
  return Status::kOk;
}

int32_t TimeZoneMinutes() noexcept {
  return __atomic_load_n(&g_timezone_minutes, __ATOMIC_ACQUIRE);
}

Result<uint64_t> LocalTimeSeconds() noexcept {
  const Result<uint64_t> wall = WallTimeSeconds();
  if (wall.is_err()) {
    return wall;
  }
  const int64_t offset =
    static_cast<int64_t>(TimeZoneMinutes()) * 60;
  const int64_t local = static_cast<int64_t>(wall.value()) + offset;
  if (local < 0) {
    return Result<uint64_t>::Err(Status::kInvalidArg);
  }
  return Result<uint64_t>::Ok(static_cast<uint64_t>(local));
}

}  // namespace og::time
