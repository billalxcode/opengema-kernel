#pragma once

// Canonical owner of formatted console output.
// Logging and drivers both go through these helpers, so formatting has
// exactly one implementation.

#include <cstdint>
#include <span>

#include "og/status.h"

namespace og {

[[nodiscard]] Status Print(std::span<const uint8_t> bytes) noexcept;
[[nodiscard]] Status PrintText(const char* text) noexcept;
[[nodiscard]] Status PrintLn(const char* text) noexcept;
[[nodiscard]] Status PrintDec(const char* label, uint64_t value) noexcept;
[[nodiscard]] Status PrintHex(const char* label, uint64_t value) noexcept;

}  // namespace og
