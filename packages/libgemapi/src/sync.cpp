#include "gemapi/sync.h"

#include "error_internal.h"
#include "kgema/sync.h"

namespace gemapi::sync {

Status wait(int* address, int expected) noexcept {
  if (address == nullptr) {
    return Status::invalid_argument;
  }
  const i64 result =
    kgema::futex_wait(reinterpret_cast<i64>(address), expected);
  if (result < 0) {
    return detail::LastError(Status::io);
  }
  return Status::ok;
}

Result<u64> wake(int* address, u64 count) noexcept {
  if (address == nullptr) {
    return Status::invalid_argument;
  }
  if (count == 0U) {
    return u64{0};
  }
  const i64 result =
    kgema::futex_wake(reinterpret_cast<i64>(address), static_cast<i64>(count));
  if (result < 0) {
    return detail::LastError(Status::io);
  }
  return static_cast<u64>(result);
}

}  // namespace gemapi::sync
