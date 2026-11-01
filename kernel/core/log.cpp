#include "og/log.h"

#include "og/print.h"

namespace og {

// Log is a semantic layer over Print: level first, then plain console
// output. Keeping the formatting in Print avoids a second digit loop.

void Log(LogLevel /*level*/, const char* text) noexcept {
  if (text == nullptr) {
    return;
  }
  (void)PrintText(text);
}

void LogDec(LogLevel /*level*/, const char* label, uint64_t value) noexcept {
  (void)PrintDec(label, value);
}

void LogHex(LogLevel /*level*/, const char* label, uint64_t value) noexcept {
  (void)PrintHex(label, value);
}

}  // namespace og
