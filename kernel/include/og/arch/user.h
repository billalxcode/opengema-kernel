#pragma once

// Canonical owner of the EL0 transition.

#include <cstdint>

namespace og::arch::aarch64 {

// Drops to EL0 at entry with stack_pointer in SP_EL0 and argc, argv, envp
// in x0, x1, x2. Never returns to the caller; the thread comes back only
// through an exception vector.
extern "C" [[noreturn]] void EnterUserMode(uint64_t entry, uint64_t stack_pointer,
                                           uint64_t argc, uint64_t argv,
                                           uint64_t envp) noexcept;

// Resumes EL0 with a saved register frame. registers points to x[0..30].
extern "C" [[noreturn]] void EnterForkedUserMode(
  const uint64_t* registers, uint64_t elr, uint64_t spsr,
  uint64_t stack_pointer) noexcept;

}  // namespace og::arch::aarch64
