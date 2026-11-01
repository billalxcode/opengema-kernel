#pragma once

// Bring-up tests for the DMA layer.

#include "og/status.h"

namespace og::dma {

[[nodiscard]] Status RunDmaSelfTest() noexcept;

}  // namespace og::dma
