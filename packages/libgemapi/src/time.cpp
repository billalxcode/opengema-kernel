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

}  // namespace gemapi::time
