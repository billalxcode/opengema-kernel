#pragma once

// Console output helpers.

#include "kgema/types.h"

namespace kgema {

void write(const char* data, usize size) noexcept;

// Array overload so string literals keep their length at compile time.
template <usize N>
inline void write(const char (&text)[N]) noexcept {
  write(text, N - 1U);
}

}  // namespace kgema
