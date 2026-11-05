#include "gemapi/time.h"

#include "kgema/time.h"

namespace gemapi::time {

u64 now_ms() noexcept {
  return kgema::now_ms();
}

Status sleep_ms(u64 milliseconds) noexcept {
  return kgema::sleep_ms(milliseconds) ? Status::ok : Status::io;
}

Result<u64> wall_seconds() noexcept {
  u64 seconds = 0;
  if (!kgema::wall_seconds(seconds)) {
    return Status::not_supported;
  }
  return seconds;
}

Status set_wall_seconds(u64 seconds) noexcept {
  return kgema::set_wall_seconds(seconds) ? Status::ok
                                           : Status::not_supported;
}

Result<u64> local_seconds() noexcept {
  u64 seconds = 0;
  if (!kgema::local_seconds(seconds)) {
    return Status::not_supported;
  }
  return seconds;
}

Status set_timezone_minutes(i64 minutes) noexcept {
  return kgema::set_timezone_minutes(minutes) ? Status::ok
                                              : Status::invalid_argument;
}

Result<i64> timezone_minutes() noexcept {
  i64 minutes = 0;
  if (!kgema::timezone_minutes(minutes)) {
    return Status::not_supported;
  }
  return minutes;
}

}  // namespace gemapi::time
