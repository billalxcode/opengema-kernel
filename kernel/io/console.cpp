#include "og/io/console.h"

#include <cstddef>

namespace og::io {

namespace {

Console g_console{};

[[nodiscard]] Status WriteRange(Console console, const char* text, std::size_t begin,
                                std::size_t end) noexcept {
  if (end <= begin) {
    return Status::kOk;
  }
  const auto* bytes = reinterpret_cast<const uint8_t*>(text + begin);
  return ConsoleWrite(console, std::span<const uint8_t>(bytes, end - begin));
}

}  // namespace

void SetConsole(Console console) noexcept {
  g_console = console;
}

Console ActiveConsole() noexcept {
  return g_console;
}

Status ConsoleWrite(Console console, std::span<const uint8_t> bytes) noexcept {
  if (!console.valid()) {
    return Status::kNotInit;
  }
  if (bytes.empty()) {
    return Status::kInvalidArg;
  }
  return console.Write(bytes);
}

Status ConsoleWriteText(Console console, const char* text) noexcept {
  if (text == nullptr) {
    return Status::kInvalidArg;
  }
  if (!console.valid()) {
    return Status::kNotInit;
  }

  // Translate newlines to CRLF so terminals do not staircase the output.
  std::size_t start = 0;
  std::size_t index = 0;
  for (;;) {
    const char value = text[index];
    if (value == '\0') {
      return WriteRange(console, text, start, index);
    }
    if (value == '\n') {
      const Status status = WriteRange(console, text, start, index);
      if (IsErr(status)) {
        return status;
      }
      constexpr char kNewline[] = "\r\n";
      const Status newline_status = ConsoleWrite(
        console, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(kNewline), 2));
      if (IsErr(newline_status)) {
        return newline_status;
      }
      start = index + 1;
    }
    ++index;
  }
}

Status ConsoleWriteActive(std::span<const uint8_t> bytes) noexcept {
  return ConsoleWrite(g_console, bytes);
}

Status ConsoleWriteTextActive(const char* text) noexcept {
  return ConsoleWriteText(g_console, text);
}

}  // namespace og::io
