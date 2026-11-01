#include "og/delay.h"

#include "og/arch/registers.h"

namespace og::time {

namespace {

constexpr uint64_t kMicrosecondsPerSecond = 1000000ULL;

[[nodiscard]] uint64_t MicrosecondsToTicks(uint64_t microseconds) noexcept {
  const uint64_t frequency = arch::aarch64::ReadCntfrqEl0();
  if (frequency == 0) {
    return 0;
  }
  return (frequency / kMicrosecondsPerSecond) * microseconds;
}

}  // namespace

void DelayUs(uint64_t microseconds) noexcept {
  if (microseconds == 0) {
    return;
  }
  const uint64_t ticks = MicrosecondsToTicks(microseconds);
  if (ticks == 0) {
    return;
  }
  const uint64_t start = arch::aarch64::ReadCntpctEl0();
  while ((arch::aarch64::ReadCntpctEl0() - start) < ticks) {
  }
}

void DelayMs(uint64_t milliseconds) noexcept {
  if (milliseconds == 0) {
    return;
  }
  DelayUs(milliseconds * 1000ULL);
}

}  // namespace og::time
