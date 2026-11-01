#include "ldgem/text.h"

#include "kgema/io.h"

namespace ldgem {

u64 TextLength(const char* text) noexcept {
  if (text == nullptr) {
    return 0;
  }
  u64 length = 0;
  while (text[length] != '\0') {
    ++length;
  }
  return length;
}

void WriteText(const char* text) noexcept {
  if (text == nullptr) {
    return;
  }
  kgema::write(text, static_cast<kgema::usize>(TextLength(text)));
}

void WriteText(const char* text, u64 size) noexcept {
  if (text == nullptr || size == 0U) {
    return;
  }
  kgema::write(text, static_cast<kgema::usize>(size));
}

void ReportSymbol(const char* prefix, const char* name) noexcept {
  WriteText(prefix);
  WriteText(name);
  WriteText("\n");
}

}  // namespace ldgem
