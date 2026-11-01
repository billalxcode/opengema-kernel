#pragma once

// Canonical owner of the console abstraction.
// A console is a plain function pointer plus context, so there is no
// vtable and no runtime support needed.

#include <cstdint>
#include <span>

#include "og/core/status.h"

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

// Dedicated kernel log sink. Usually the serial console, so the graphical
// display only shows application output and TTY echo.
void SetLogConsole(Console console) noexcept;
[[nodiscard]] Console LogConsole() noexcept;

// Instance based helpers.
[[nodiscard]] Status ConsoleWrite(Console console, std::span<const uint8_t> bytes) noexcept;
[[nodiscard]] Status ConsoleWriteText(Console console, const char* text) noexcept;

// Active console helpers.
[[nodiscard]] Status ConsoleWriteActive(std::span<const uint8_t> bytes) noexcept;
[[nodiscard]] Status ConsoleWriteTextActive(const char* text) noexcept;

// Forwards to two consoles, for example serial plus framebuffer. The owner
// keeps the Tee alive and hands console() to SetConsole.
class Tee {
 public:
  constexpr Tee() noexcept = default;
  constexpr Tee(Console first, Console second) noexcept
    : first_(first), second_(second) {}

  void Set(Console first, Console second) noexcept {
    first_ = first;
    second_ = second;
  }

  [[nodiscard]] Console console() noexcept { return Console(&WriteTrampoline, this); }

  [[nodiscard]] Status write(std::span<const uint8_t> bytes) noexcept {
    if (bytes.empty()) {
      return Status::kInvalidArg;
    }
    if (!first_.valid() && !second_.valid()) {
      return Status::kNotInit;
    }
    const Status first = first_.valid() ? first_.Write(bytes) : Status::kOk;
    const Status second = second_.valid() ? second_.Write(bytes) : Status::kOk;
    return IsErr(first) ? first : second;
  }

 private:
  static Status WriteTrampoline(void* context, std::span<const uint8_t> bytes) noexcept {
    if (context == nullptr) {
      return Status::kInvalidArg;
    }
    return static_cast<Tee*>(context)->write(bytes);
  }

  Console first_{};
  Console second_{};
};

}  // namespace og::io
