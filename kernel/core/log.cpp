#include "og/core/log.h"

#include "format.h"
#include "og/io/console.h"

namespace og {

namespace {

constexpr std::size_t kMaxLine = 128;

void WriteLabeled(const char* label, uint64_t value, bool hex) noexcept {
  char buffer[kMaxLine];
  fmt::LineBuilder line(buffer, sizeof(buffer));
  if (label != nullptr && label[0] != '\0') {
    line.Text(label);
    line.Text(": ");
  }
  if (hex) {
    line.Hex(value);
  } else {
    line.Dec(value);
  }
  line.Char('\n');
  (void)io::ConsoleWriteText(io::LogConsole(), line.c_str());
}

}  // namespace

void Log(LogLevel /*level*/, const char* text) noexcept {
  if (text == nullptr) {
    return;
  }
  (void)io::ConsoleWriteText(io::LogConsole(), text);
}

void LogDec(LogLevel /*level*/, const char* label, uint64_t value) noexcept {
  WriteLabeled(label, value, false);
}

void LogHex(LogLevel /*level*/, const char* label, uint64_t value) noexcept {
  WriteLabeled(label, value, true);
}

}  // namespace og
