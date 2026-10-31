#pragma once

// Canonical owner of fatal error handling.

namespace og {

// Prints reason and CPU fault registers, then halts with interrupts off.
// Never returns.
[[noreturn]] void Panic(const char* reason) noexcept;

}  // namespace og
