#pragma once

// Private helper for API wrappers. Public headers must not include kgema.

#include "gemapi/error.h"
#include "kgema/syscall.h"

namespace gemapi::detail {

[[nodiscard]] inline Status LastError(
  Status fallback = Status::unknown) noexcept {
  const i64 error = kgema::syscall::last_error();
  return error < 0 ? from_errno(error) : fallback;
}

}  // namespace gemapi::detail
