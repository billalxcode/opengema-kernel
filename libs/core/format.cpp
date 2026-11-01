#include "format.h"

namespace og::fmt {

namespace {

void AppendByte(char* buffer, std::size_t capacity, std::size_t& length, char value) noexcept {
  if (buffer == nullptr) {
    return;
  }
  if (capacity < 2) {
    return;
  }
  if (length + 1 >= capacity) {
    return;
  }
  buffer[length] = value;
  ++length;
  buffer[length] = '\0';
}

}  // namespace

LineBuilder::LineBuilder(char* buffer, std::size_t capacity) noexcept
  : buffer_(buffer), capacity_(capacity), length_(0) {
  if (buffer_ == nullptr) {
    return;
  }
  if (capacity_ == 0) {
    return;
  }
  buffer_[0] = '\0';
}

LineBuilder& LineBuilder::Char(char value) noexcept {
  AppendByte(buffer_, capacity_, length_, value);
  return *this;
}

LineBuilder& LineBuilder::Text(const char* text) noexcept {
  if (text == nullptr) {
    return *this;
  }
  while (*text != '\0') {
    Char(*text);
    ++text;
  }
  return *this;
}

LineBuilder& LineBuilder::Dec(uint64_t value) noexcept {
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
    Char(digits[count]);
  }
  return *this;
}

LineBuilder& LineBuilder::Hex(uint64_t value) noexcept {
  constexpr char kDigits[] = "0123456789abcdef";
  Char('0');
  Char('x');
  for (int shift = 60; shift >= 0; shift -= 4) {
    const std::size_t index = static_cast<std::size_t>((value >> shift) & 0xFU);
    Char(kDigits[index]);
  }
  return *this;
}

}  // namespace og::fmt
