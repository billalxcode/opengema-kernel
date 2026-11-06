#pragma once

// Kernel random source for address space layout randomization and other
// non-cryptographic boot-time choices.

#include <cstdint>

namespace og::core {

[[nodiscard]] uint64_t Random64() noexcept;

}  // namespace og::core
