#pragma once

// Canonical owner of logging. Sinks are registered by boot code, so this
// header stays free of any driver dependency.

#include <cstdint>

namespace og {

enum class LogLevel : uint8_t {
  kError = 0,
  kWarn = 1,
  kInfo = 2,
  kDebug = 3,
};

using LogSink = void (*)(void* context, LogLevel level, const char* text);

// Safe before any sink is set: messages are dropped.
void SetLogSink(LogSink sink, void* context) noexcept;

void Log(LogLevel level, const char* text) noexcept;
void LogDec(LogLevel level, const char* label, uint64_t value) noexcept;
void LogHex(LogLevel level, const char* label, uint64_t value) noexcept;

}  // namespace og
