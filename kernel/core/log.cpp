#include "og/core/log.h"

#include "format.h"
#include "memory.h"
#include "og/core/log_ring.h"
#include "og/io/console.h"

namespace og {

namespace {

constexpr std::size_t kMaxLine = 128;
LogLevel g_level = LogLevel::kInfo;

void WriteLabeled(LogLevel level, const char* label, uint64_t value,
                  bool hex) noexcept {
  if (!LogEnabled(level)) {
    return;
  }
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
  log_ring::Append(line.c_str(), static_cast<uint32_t>(line.size()));
  (void)io::ConsoleWriteText(io::LogConsole(), line.c_str());
}

}  // namespace

void SetLogLevel(LogLevel level) noexcept {
  g_level = level;
}

LogLevel GetLogLevel() noexcept {
  return g_level;
}

bool LogEnabled(LogLevel level) noexcept {
  return static_cast<uint8_t>(level) <= static_cast<uint8_t>(g_level);
}

void Log(LogLevel level, const char* text) noexcept {
  if (text == nullptr || !LogEnabled(level)) {
    return;
  }
  log_ring::Append(text, static_cast<uint32_t>(mem::Length(text)));
  (void)io::ConsoleWriteText(io::LogConsole(), text);
}

void LogDec(LogLevel level, const char* label, uint64_t value) noexcept {
  WriteLabeled(level, label, value, false);
}

void LogHex(LogLevel level, const char* label, uint64_t value) noexcept {
  WriteLabeled(level, label, value, true);
}

}  // namespace og
