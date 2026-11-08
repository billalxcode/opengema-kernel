#pragma once

// Boot composition helpers. These run at rank 15 and may pull every module
// together, but they keep kmain small.

#include "og/core/status.h"

namespace og::boot {

void InitDevices() noexcept;
void InitFilesystem() noexcept;

void RunTimerSelfTest() noexcept;
void RunSchedulerSelfTest() noexcept;
void RunAsidSelfTest() noexcept;
void RunAddressSpaceSelfTest() noexcept;
void RunUserSelfTest() noexcept;

[[nodiscard]] uint64_t TlbSyncCount() noexcept;
[[nodiscard]] uint64_t TlbTimeoutCount() noexcept;
[[nodiscard]] uint64_t IpiCount(uint32_t cpu) noexcept;

}  // namespace og::boot
