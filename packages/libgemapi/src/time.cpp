#include "gemapi/time.h"

#include "kgema/time.h"

namespace gemapi::time {

u64 now_ms() noexcept {
  return kgema::now_ms();
}

Status sleep_ms(u64 milliseconds) noexcept {
  return kgema::sleep_ms(milliseconds) ? Status::ok : Status::io;
}

}  // namespace gemapi::time
