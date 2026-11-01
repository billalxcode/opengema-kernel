#include "og/timer.h"

#include "og/arch/barriers.h"
#include "og/arch/registers.h"
#include "og/irq.h"

namespace og::time {

namespace {

uint64_t g_frequency = 0;
uint64_t g_period = 0;
uint64_t g_ticks = 0;
bool g_ready = false;
TickCallback g_tick_callback = nullptr;
void* g_tick_context = nullptr;

constexpr uint64_t kCntpCtlEnable = 1ULL << 0U;
constexpr uint64_t kCntpCtlMask = 1ULL << 1U;

[[nodiscard]] Status TimerHandler(void* /*context*/, uint32_t /*irq*/) noexcept {
  if (g_period != 0) {
    arch::aarch64::WriteCntpTvalEl0(g_period);
  }
  ++g_ticks;
  if (g_tick_callback != nullptr) {
    g_tick_callback(g_tick_context);
  }
  return Status::kOk;
}

}  // namespace

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
  g_ticks = 0;
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
  return g_ticks;
}

void TimerSetTickCallback(TickCallback callback, void* context) noexcept {
  g_tick_callback = callback;
  g_tick_context = context;
}

uint64_t TimerFrequency() noexcept {
  return g_frequency;
}

}  // namespace og::time
