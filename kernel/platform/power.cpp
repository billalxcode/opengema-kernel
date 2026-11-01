#include "og/platform/power.h"

#include "og/acpi/acpi.h"
#include "og/core/log.h"
#include "og/io/mmio.h"
#include "og/platform/psci.h"

namespace og::platform {

namespace {

// ACPI reset register is a generic address structure. Only system memory is
// used on ARM64; the write normally resets the machine before returning.
[[nodiscard]] Status ResetViaFadt(const acpi::Info& info) noexcept {
  if (info.reset_register_address == 0U) {
    return Status::kUnsupported;
  }
  if (info.reset_register_space != 0U) {
    return Status::kUnsupported;
  }
  io::MmioRegion region(info.reset_register_address, 4);
  const Status status = region.Write8(0, info.reset_value);
  if (IsErr(status)) {
    return status;
  }
  return Status::kIoError;
}

}  // namespace

Status Shutdown() noexcept {
  if (acpi::Ready() && !acpi::GetInfo().psci_compliant) {
    return Status::kUnsupported;
  }
  if (!psci::Ready()) {
    return Status::kNotInit;
  }
  Log(LogLevel::kInfo, "power: shutdown via psci\n");
  return psci::SystemOff();
}

Status Reboot() noexcept {
  if (acpi::Ready()) {
    const acpi::Info& info = acpi::GetInfo();
    if (info.reset_register_supported) {
      Log(LogLevel::kInfo, "power: reboot via fadt\n");
      return ResetViaFadt(info);
    }
    if (!info.psci_compliant) {
      return Status::kUnsupported;
    }
  }
  if (!psci::Ready()) {
    return Status::kNotInit;
  }
  Log(LogLevel::kInfo, "power: reboot via psci\n");
  return psci::SystemReset();
}

}  // namespace og::platform
