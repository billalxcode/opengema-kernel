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

enum class LogLevel : u64 {
  error = 0,
  warn = 1,
  info = 2,
  debug = 3,
};

[[nodiscard]] constexpr Version version() noexcept {
  return Version{kVersionMajor, kVersionMinor};
}

[[nodiscard]] LogLevel log_level() noexcept;
[[nodiscard]] Status set_log_level(LogLevel level) noexcept;

// These return only when the kernel rejects the request.
[[nodiscard]] Status shutdown() noexcept;
[[nodiscard]] Status reboot() noexcept;

}  // namespace gemapi::system
