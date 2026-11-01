#pragma once

// Canonical owner of the EL0 transition.

#include <cstdint>

namespace og::arch::aarch64 {

// Drops to EL0 at entry with stack_pointer in SP_EL0. Never returns to the
// caller; the thread comes back only through an exception vector.
// C linkage because the implementation is assembly.
extern "C" [[noreturn]] void EnterUserMode(uint64_t entry, uint64_t stack_pointer) noexcept;

}  // namespace og::arch::aarch64
