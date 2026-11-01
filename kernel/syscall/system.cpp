#include "og/syscall/syscall_internal.h"

#include "og/core/power.h"

namespace og::syscall {

Status DoShutdown(uint64_t& result) noexcept {
  result = 0;
  return power::Shutdown();
}

Status DoReboot(uint64_t& result) noexcept {
  result = 0;
  return power::Reboot();
}

}  // namespace og::syscall
