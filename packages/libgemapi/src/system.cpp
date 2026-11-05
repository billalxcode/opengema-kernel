#include "gemapi/system.h"

#include "kgema/system.h"

namespace gemapi::system {

LogLevel log_level() noexcept {
  const i64 value = kgema::get_log_level();
  if (value < 0 || value > 3) {
    return LogLevel::info;
  }
  return static_cast<LogLevel>(value);
}

Status set_log_level(LogLevel level) noexcept {
  const i64 value = static_cast<i64>(level);
  if (value < 0 || value > 3) {
    return Status::invalid_argument;
  }
  return kgema::set_log_level(value) < 0 ? Status::not_supported
                                         : Status::ok;
}

Status shutdown() noexcept {
  return kgema::shutdown() < 0 ? Status::not_supported : Status::ok;
}

Status reboot() noexcept {
  return kgema::reboot() < 0 ? Status::not_supported : Status::ok;
}

}  // namespace gemapi::system
