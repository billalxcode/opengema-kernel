#pragma once

// Status codes and a small Result type for every fallible API call.
//
// The kernel ABI still reports a plain -1 for most failures, so libgemapi
// maps errors by operation: a failed open reports not_found, a failed mmap
// reports no_memory, and so on. Fine grained codes arrive when the kernel
// starts returning real error values.

#include "gemapi/types.h"

namespace gemapi {

enum class Status : i64 {
  ok = 0,
  invalid_argument = 1,
  not_found = 2,
  denied = 3,
  busy = 4,
  io = 5,
  no_memory = 6,
  not_supported = 7,
  unknown = 8,
};

[[nodiscard]] const char* to_string(Status status) noexcept;

template <typename T>
class [[nodiscard]] Result {
 public:
  constexpr Result(T value) noexcept
      : value_(static_cast<T&&>(value)), status_(Status::ok) {}
  constexpr Result(Status status) noexcept : value_(T{}), status_(status) {}

  [[nodiscard]] constexpr bool ok() const noexcept {
    return status_ == Status::ok;
  }
  [[nodiscard]] constexpr explicit operator bool() const noexcept { return ok(); }
  [[nodiscard]] constexpr Status status() const noexcept { return status_; }

  [[nodiscard]] constexpr T& value() & noexcept { return value_; }
  [[nodiscard]] constexpr const T& value() const& noexcept { return value_; }
  [[nodiscard]] constexpr T&& value() && noexcept { return static_cast<T&&>(value_); }
  [[nodiscard]] constexpr T value_or(T fallback) const noexcept {
    return ok() ? value_ : fallback;
  }

 private:
  T value_;
  Status status_;
};

}  // namespace gemapi
