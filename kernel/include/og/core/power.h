#pragma once

// System power requests. The platform layer installs the handlers once PSCI
// and ACPI are ready, so the syscall layer never depends on platform code.

#include "og/core/status.h"

namespace og::power {

using Handler = Status (*)() noexcept;

void SetHandlers(Handler shutdown, Handler reboot) noexcept;

// Return only when the request could not be started.
[[nodiscard]] Status Shutdown() noexcept;
[[nodiscard]] Status Reboot() noexcept;

}  // namespace og::power
