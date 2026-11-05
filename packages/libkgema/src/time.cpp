#include "kgema/time.h"

#include "kgema/syscall.h"

namespace kgema {

u64 now_ms() noexcept {
  const i64 result = syscall::invoke3(syscall::kTime, 0, 0, 0);
  return result < 0 ? 0U : static_cast<u64>(result);
}

bool sleep_ms(u64 milliseconds) noexcept {
  return syscall::invoke3(syscall::kSleep,
                          static_cast<i64>(milliseconds), 0, 0) == 0;
}

bool wall_seconds(u64& seconds) noexcept {
  const i64 result = syscall::invoke3(syscall::kWallTime, 0, 0, 0);
  if (result < 0) {
    return false;
  }
  seconds = static_cast<u64>(result);
  return true;
}

bool set_wall_seconds(u64 seconds) noexcept {
  return syscall::invoke3(syscall::kSetWallTime,
                          static_cast<i64>(seconds), 0, 0) == 0;
}

bool local_seconds(u64& seconds) noexcept {
  const i64 result = syscall::invoke3(syscall::kLocalTime, 0, 0, 0);
  if (result < 0) {
    return false;
  }
  seconds = static_cast<u64>(result);
  return true;
}

bool set_timezone_minutes(i64 minutes) noexcept {
  return syscall::invoke3(syscall::kSetTimeZone, minutes, 0, 0) == 0;
}

bool timezone_minutes(i64& minutes) noexcept {
  constexpr i64 kOffsetBias = 840;
  const i64 result = syscall::invoke3(syscall::kGetTimeZone, 0, 0, 0);
  if (result < 0) {
    return false;
  }
  minutes = result - kOffsetBias;
  return true;
}

}  // namespace kgema
