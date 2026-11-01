#pragma once

// Canonical owner of the callee-saved context switch ABI.

#include <cstdint>

extern "C" {

// Saves callee-saved registers on the current stack, stores the stack
// pointer in *old_sp, then loads new_sp and returns into the new context.
void SwitchContext(uint64_t* old_sp, uint64_t new_sp);

// Entry point for a brand new thread. Expects x19 to hold the thread entry
// and x20 the argument. Enables IRQ before calling into C++.
void ThreadStartTrampoline();

// Returns after the thread entry function returns.
void SchedulerThreadExit() noexcept;

}  // extern "C"
