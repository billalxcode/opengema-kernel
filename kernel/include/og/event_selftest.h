#pragma once

// Bring-up tests for the event subsystem.

#include "og/status.h"

namespace og::events {

[[nodiscard]] Status RunEventSelfTest() noexcept;

}  // namespace og::events
