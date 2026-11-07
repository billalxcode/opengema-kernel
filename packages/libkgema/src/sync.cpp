#include "kgema/sync.h"

#include "kgema/syscall.h"

namespace kgema {

i64 futex_wait(i64 address, i64 expected) noexcept {
  return syscall::invoke3(syscall::kFutex, address, kFutexWait, expected);
}

i64 futex_wake(i64 address, i64 count) noexcept {
  if (count < 0) {
    return syscall::kErrInvalid;
  }
  return syscall::invoke3(syscall::kFutex, address, kFutexWake, count);
}

}  // namespace kgema
