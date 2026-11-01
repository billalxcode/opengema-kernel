#pragma once

// Platform power operations selected from ACPI and PSCI capabilities.

#include "og/core/status.h"

namespace og::platform {

// These do not return when the platform accepts them.
[[nodiscard]] Status Shutdown() noexcept;
[[nodiscard]] Status Reboot() noexcept;

}  // namespace og::platform
