#pragma once

// Canonical owner of logging. Formatting and output are delegated to
// og/print.h so there is one implementation of both.

#include <cstdint>

namespace og {

enum class LogLevel : uint8_t {
  kError = 0,
  kWarn = 1,
  kInfo = 2,
  kDebug = 3,
};

// Safe before a console is set: messages are dropped.
void Log(LogLevel level, const char* text) noexcept;
void LogDec(LogLevel level, const char* label, uint64_t value) noexcept;
void LogHex(LogLevel level, const char* label, uint64_t value) noexcept;

}  // namespace og
