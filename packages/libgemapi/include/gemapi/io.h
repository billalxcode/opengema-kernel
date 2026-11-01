#pragma once

// Console input and output. No application writes to a device directly.

#include "gemapi/error.h"
#include "gemapi/types.h"

namespace gemapi::io {

// Writes to standard output. Best effort: console errors are not reported
// because there is no useful recovery from a failed console write.
void write(const char* data, usize size) noexcept;

// Array overload so string literals keep their length at compile time.
template <usize N>
inline void write(const char (&text)[N]) noexcept {
  write(text, N - 1U);
}

// Writes a NUL terminated string.
void print(const char* text) noexcept;

// Reads one line from standard input, without the trailing newline.
// Returns the number of bytes stored, or a status when the read failed.
[[nodiscard]] Result<usize> read_line(char* buffer, usize capacity) noexcept;

}  // namespace gemapi::io
