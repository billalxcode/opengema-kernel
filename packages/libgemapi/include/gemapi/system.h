#pragma once

// System information exposed to applications.

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

}  // namespace gemapi::system
