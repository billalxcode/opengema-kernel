#include "og/log.h"

#include <cstddef>

namespace og {

namespace {

LogSink g_sink = nullptr;
void* g_context = nullptr;

constexpr std::size_t kMaxLine = 128;
constexpr char kHexDigits[] = "0123456789abcdef";

char* AppendChar(char* cursor, const char* end, char value) {
  if (cursor >= end) {
    return cursor;
  }
  *cursor = value;
  return cursor + 1;
}

char* AppendText(char* cursor, const char* end, const char* text) {
  if (text == nullptr) {
    return cursor;
  }
  while (*text != '\0') {
    cursor = AppendChar(cursor, end, *text);
    ++text;
  }
  return cursor;
}

char* AppendDec(char* cursor, const char* end, uint64_t value) {
  char digits[20];
  std::size_t count = 0;
  if (value == 0U) {
    digits[count] = '0';
    ++count;
  }
  while (value != 0U) {
    digits[count] = static_cast<char>('0' + (value % 10U));
    ++count;
    value /= 10U;
  }
  while (count > 0) {
    --count;
    cursor = AppendChar(cursor, end, digits[count]);
  }
  return cursor;
}

char* AppendHex(char* cursor, const char* end, uint64_t value) {
  cursor = AppendChar(cursor, end, '0');
  cursor = AppendChar(cursor, end, 'x');
  for (int shift = 60; shift >= 0; shift -= 4) {
    const std::size_t nibble = static_cast<std::size_t>((value >> shift) & 0xFU);
    cursor = AppendChar(cursor, end, kHexDigits[nibble]);
  }
  return cursor;
}

char* AppendLabel(char* cursor, const char* end, const char* label) {
  if (label == nullptr) {
    return cursor;
  }
  cursor = AppendText(cursor, end, label);
  cursor = AppendChar(cursor, end, ':');
  cursor = AppendChar(cursor, end, ' ');
  return cursor;
}

void Emit(LogLevel level, const char* text) noexcept {
  if (g_sink == nullptr) {
    return;
  }
  if (text == nullptr) {
    return;
  }
  g_sink(g_context, level, text);
}

}  // namespace

void SetLogSink(LogSink sink, void* context) noexcept {
  g_sink = sink;
  g_context = context;
}

void Log(LogLevel level, const char* text) noexcept {
  Emit(level, text);
}

void LogDec(LogLevel level, const char* label, uint64_t value) noexcept {
  if (g_sink == nullptr) {
    return;
  }
  char line[kMaxLine];
  char* const begin = line;
  char* const end = line + (kMaxLine - 1);
  char* cursor = AppendLabel(begin, end, label);
  cursor = AppendDec(cursor, end, value);
  cursor = AppendChar(cursor, end, '\n');
  *cursor = '\0';
  Emit(level, begin);
}

void LogHex(LogLevel level, const char* label, uint64_t value) noexcept {
  if (g_sink == nullptr) {
    return;
  }
  char line[kMaxLine];
  char* const begin = line;
  char* const end = line + (kMaxLine - 1);
  char* cursor = AppendLabel(begin, end, label);
  cursor = AppendHex(cursor, end, value);
  cursor = AppendChar(cursor, end, '\n');
  *cursor = '\0';
  Emit(level, begin);
}

}  // namespace og
