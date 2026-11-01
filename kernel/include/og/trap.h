#pragma once

// Canonical owner of the exception vector table installation.
// The table itself lives in vectors.S, which is arch owned.

namespace og::events {

void InstallVectorTable() noexcept;

}  // namespace og::events
