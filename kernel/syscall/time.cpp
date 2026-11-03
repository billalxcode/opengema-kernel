#include "og/syscall/syscall_internal.h"

#include "og/sched/sched.h"
#include "og/time/timer.h"

namespace og::syscall {

namespace {

// Cap a single sleep so the tick to nanosecond conversion stays safe.
constexpr uint64_t kMaxSleepMs = 24U * 60U * 60U * 1000U;

}  // namespace

Status DoTime(uint64_t& result) noexcept {
  result = time::TimeNowMs();
  return Status::kOk;
}

Status DoWallTime(uint64_t& result) noexcept {
  const Result<uint64_t> seconds = time::WallTimeSeconds();
  if (seconds.is_err()) {
    return seconds.status();
  }
  result = seconds.value();
  return Status::kOk;
}

Status DoSetWallTime(uint64_t seconds, uint64_t& result) noexcept {
  const Status status = time::SetWallTimeSeconds(seconds);
  if (IsErr(status)) {
    return status;
  }
  result = 0;
  return Status::kOk;
}

Status DoSleep(uint64_t milliseconds, uint64_t& result) noexcept {
  if (milliseconds > kMaxSleepMs) {
    return Status::kInvalidArg;
  }
  const Status status = sched::ThreadSleep(milliseconds);
  if (IsErr(status)) {
    return status;
  }
  result = 0;
  return Status::kOk;
}

}  // namespace og::syscall
