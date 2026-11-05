#pragma once

// Bounded kernel log ring used by /proc/dmesg. Serial output stays the
// primary sink; this ring keeps the most recent lines for user space.

#include <cstdint>

namespace og::log_ring {

// Copies up to capacity bytes starting at offset from the oldest entry.
// Returns the number of bytes copied.
[[nodiscard]] uint32_t Read(uint64_t offset, char* buffer,
                            uint32_t capacity) noexcept;

// Number of valid bytes currently stored.
[[nodiscard]] uint32_t Size() noexcept;

// Appends raw log bytes, overwriting the oldest data when full.
void Append(const char* text, uint32_t length) noexcept;

}  // namespace og::log_ring
