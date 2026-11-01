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

}  // namespace kgema
