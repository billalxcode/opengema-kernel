#pragma once

// Canonical owner of status codes and fallible results.
// No module may invent its own error enum.

#include <cstdint>

namespace og {

enum class Status : uint32_t {
  kOk = 0,
  kInvalidArg = 1,
  kNotInit = 2,
  kNoDevice = 3,
  kNoMem = 4,
  kUnaligned = 5,
  kNotImpl = 6,
  kNotFound = 7,
  kExists = 8,
  kTimeout = 9,
  kIoError = 10,
  kUnsupported = 11,
  kBusy = 12,
};

[[nodiscard]] constexpr bool IsOk(Status status) noexcept {
  return status == Status::kOk;
}

[[nodiscard]] constexpr bool IsErr(Status status) noexcept {
  return status != Status::kOk;
}

// Result holds a value or an error, never both. T must be default
// constructible because the error path still owns a value slot.
template <typename T>
class Result {
 public:
  Result() noexcept : value_{}, status_(Status::kNotInit) {}

  [[nodiscard]] static Result Ok(T value) noexcept {
    return Result(value, Status::kOk);
  }

  [[nodiscard]] static Result Err(Status status) noexcept {
    if (status == Status::kOk) {
      return Result(T{}, Status::kInvalidArg);
    }
    return Result(T{}, status);
  }

  [[nodiscard]] bool is_ok() const noexcept { return status_ == Status::kOk; }
  [[nodiscard]] bool is_err() const noexcept { return status_ != Status::kOk; }
  [[nodiscard]] Status status() const noexcept { return status_; }

  // Precondition: is_ok(). Callers must check first.
  [[nodiscard]] T& value() noexcept { return value_; }
  [[nodiscard]] const T& value() const noexcept { return value_; }

 private:
  Result(T value, Status status) noexcept : value_(value), status_(status) {}

  T value_;
  Status status_;
};

}  // namespace og
