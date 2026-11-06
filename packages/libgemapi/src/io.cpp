#include "gemapi/io.h"

#include "error_internal.h"
#include "kgema/file.h"
#include "kgema/io.h"

namespace gemapi::io {

void write(const char* data, usize size) noexcept {
  if (data == nullptr || size == 0U) {
    return;
  }
  kgema::write(data, size);
}

void print(const char* text) noexcept {
  if (text == nullptr) {
    return;
  }
  usize length = 0;
  while (text[length] != '\0') {
    ++length;
  }
  kgema::write(text, length);
}

Result<usize> read_line(char* buffer, usize capacity) noexcept {
  if (buffer == nullptr || capacity < 2U) {
    return Status::invalid_argument;
  }
  const kgema::i64 count = kgema::read(0, buffer, capacity - 1U);
  if (count < 0) {
    return detail::LastError(Status::io);
  }
  usize length = static_cast<usize>(count);
  if (length >= capacity) {
    length = capacity - 1U;
  }
  if (length > 0U && buffer[length - 1U] == '\n') {
    --length;
  }
  buffer[length] = '\0';
  return length;
}

}  // namespace gemapi::io
