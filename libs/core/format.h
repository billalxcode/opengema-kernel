#pragma once

// Canonical owner of text formatting into a fixed buffer.
// No allocation, no varargs, no duplicate digit loops elsewhere.

#include <cstddef>
#include <cstdint>

namespace og::fmt {

class LineBuilder {
 public:
  LineBuilder(char* buffer, std::size_t capacity) noexcept;

  LineBuilder& Text(const char* text) noexcept;
  LineBuilder& Char(char value) noexcept;
  LineBuilder& Dec(uint64_t value) noexcept;
  LineBuilder& Hex(uint64_t value) noexcept;

  [[nodiscard]] const char* c_str() const noexcept { return buffer_; }
  [[nodiscard]] std::size_t size() const noexcept { return length_; }
  [[nodiscard]] bool empty() const noexcept { return length_ == 0; }

 private:
  char* buffer_;
  std::size_t capacity_;
  std::size_t length_;
};

}  // namespace og::fmt
