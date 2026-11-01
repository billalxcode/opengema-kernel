#pragma once

// Bring-up tests for the memory subsystem. Runs once during kmain.

#include "og/core/status.h"

namespace og::mem {

[[nodiscard]] Status RunSelfTest() noexcept;

}  // namespace og::mem
