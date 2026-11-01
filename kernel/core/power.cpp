#include "og/core/power.h"

namespace og::power {

namespace {

Handler g_shutdown = nullptr;
Handler g_reboot = nullptr;

}  // namespace

void SetHandlers(Handler shutdown, Handler reboot) noexcept {
  g_shutdown = shutdown;
  g_reboot = reboot;
}

Status Shutdown() noexcept {
  if (g_shutdown == nullptr) {
    return Status::kNotInit;
  }
  return g_shutdown();
}

Status Reboot() noexcept {
  if (g_reboot == nullptr) {
    return Status::kNotInit;
  }
  return g_reboot();
}

}  // namespace og::power
