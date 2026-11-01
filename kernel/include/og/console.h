#pragma once

// Canonical owner of the console abstraction.
// A console is a plain function pointer plus context, so there is no
// vtable and no runtime support needed.

#include <cstdint>
#include <span>

#include "og/status.h"

namespace og::io {

class Console {
 public:
  using WriteFn = Status (*)(void* context, std::span<const uint8_t> bytes);

  constexpr Console() noexcept = default;
  constexpr Console(WriteFn write, void* context) noexcept
    : write_(write), context_(context) {}

  [[nodiscard]] bool valid() const noexcept { return write_ != nullptr; }

  [[nodiscard]] Status Write(std::span<const uint8_t> bytes) const noexcept {
    if (write_ == nullptr) {
      return Status::kNotInit;
    }
    return write_(context_, bytes);
  }

 private:
  WriteFn write_ = nullptr;
  void* context_ = nullptr;
};

// Active console singleton. Null means output is dropped, which is the
// valid state before any driver is up.
void SetConsole(Console console) noexcept;
[[nodiscard]] Console ActiveConsole() noexcept;

// Instance based helpers.
[[nodiscard]] Status ConsoleWrite(Console console, std::span<const uint8_t> bytes) noexcept;
[[nodiscard]] Status ConsoleWriteText(Console console, const char* text) noexcept;

// Active console helpers.
[[nodiscard]] Status ConsoleWriteActive(std::span<const uint8_t> bytes) noexcept;
[[nodiscard]] Status ConsoleWriteTextActive(const char* text) noexcept;

}  // namespace og::io
