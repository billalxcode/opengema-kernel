#pragma once

// Shared declarations for syscall handler categories. Private to the
// kernel; not part of the user ABI.

#include <cstdint>

#include "og/core/status.h"
#include "og/mem/vmm.h"
#include "og/process/process.h"

namespace og::syscall::detail {

constexpr uint64_t kMaxWriteChunk = 256;
constexpr uint64_t kMaxReadChunk = 256;
constexpr uint64_t kTtyLine = 128;
constexpr uint64_t kStdout = 1;

[[nodiscard]] process::Process* CurrentProcess() noexcept;

[[nodiscard]] Status TtyReadLine(const mem::AddressSpace& space,
                                 uint64_t user_buffer, uint64_t length,
                                 uint64_t& result) noexcept;

}  // namespace og::syscall::detail

namespace og::syscall {

[[nodiscard]] Status DoRead(uint64_t descriptor, uint64_t user_buffer,
                            uint64_t length, uint64_t& result) noexcept;

[[nodiscard]] Status DoWrite(uint64_t descriptor, uint64_t user_buffer,
                             uint64_t length, uint64_t& result) noexcept;

}  // namespace og::syscall
