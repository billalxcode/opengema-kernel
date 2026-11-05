#pragma once

// Canonical owner of the exception vector table installation.
// The table itself lives in vectors.S, which is arch owned.

#include "og/arch/trap.h"

namespace og::events {

using UserFaultHook = void (*)(arch::aarch64::TrapFrame* frame) noexcept;

void InstallVectorTable() noexcept;

// Called for a synchronous exception from lower EL that is not a handled
// syscall. The hook must terminate the current user process.
void SetUserFaultHook(UserFaultHook hook) noexcept;

}  // namespace og::events
