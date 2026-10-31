#pragma once

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
};

[[nodiscard]] constexpr bool IsOk(Status s) noexcept {
  return s == Status::kOk;
}

}  // namespace og
