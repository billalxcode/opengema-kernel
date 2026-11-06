#pragma once

// Canonical owner of the exception vector table installation.
// The table itself lives in vectors.S, which is arch owned.

#include "og/arch/trap.h"
#include "og/core/status.h"

namespace og::events {

// Returns kOk when the fault was handled and the instruction should be
// retried. Any error terminates the current process.
using UserFaultHook = Status (*)(arch::aarch64::TrapFrame* frame) noexcept;

void InstallVectorTable() noexcept;

// Called for a synchronous exception from lower EL that is not a handled
// syscall. A hook that returns an error must terminate the current process.
void SetUserFaultHook(UserFaultHook hook) noexcept;

}  // namespace og::events
