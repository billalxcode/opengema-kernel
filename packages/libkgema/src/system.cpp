#include "kgema/system.h"

#include "kgema/syscall.h"

namespace kgema {

i64 shutdown() noexcept {
  return syscall::invoke3(syscall::kShutdown, 0, 0, 0);
}

i64 reboot() noexcept {
  return syscall::invoke3(syscall::kReboot, 0, 0, 0);
}

i64 get_log_level() noexcept {
  return syscall::invoke3(syscall::kGetLogLevel, 0, 0, 0);
}

i64 set_log_level(i64 level) noexcept {
  return syscall::invoke3(syscall::kSetLogLevel, level, 0, 0);
}

}  // namespace kgema
