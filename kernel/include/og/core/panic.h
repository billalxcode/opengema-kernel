#pragma once

// Canonical owner of fatal error handling.

namespace og {

// Optional hook for subsystem-specific panic context, such as the current
// thread, process, and memory counters. Must not return and must not block.
using PanicInfoHook = void (*)() noexcept;
void SetPanicInfoHook(PanicInfoHook hook) noexcept;

// Prints reason and CPU fault registers, then halts with interrupts off.
// Never returns.
[[noreturn]] void Panic(const char* reason) noexcept;

}  // namespace og
