#pragma once

// System information exposed to applications.

#include "gemapi/error.h"
#include "gemapi/types.h"

namespace gemapi::system {

constexpr u32 kVersionMajor = 0;
constexpr u32 kVersionMinor = 1;

struct Version {
  u32 major;
  u32 minor;
};

[[nodiscard]] constexpr Version version() noexcept {
  return Version{kVersionMajor, kVersionMinor};
}

// These return only when the kernel rejects the request.
[[nodiscard]] Status shutdown() noexcept;
[[nodiscard]] Status reboot() noexcept;

}  // namespace gemapi::system
