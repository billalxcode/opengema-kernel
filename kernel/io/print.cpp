#include "og/io/print.h"

#include "format.h"
#include "og/arch/spinlock.h"
#include "og/io/console.h"

namespace og {

namespace {

constexpr std::size_t kMaxLine = 128;

arch::aarch64::IrqSpinlock g_print_lock;

[[nodiscard]] const char* LabelOrEmpty(const char* label) noexcept {
  return label == nullptr ? "" : label;
}

// Caller holds g_print_lock.
[[nodiscard]] Status PrintLabeled(const char* label, uint64_t value, bool hex) noexcept {
  char buffer[kMaxLine];
  fmt::LineBuilder line(buffer, sizeof(buffer));
  const char* text = LabelOrEmpty(label);
  if (*text != '\0') {
    line.Text(text);
    line.Text(": ");
  }
  if (hex) {
    line.Hex(value);
  } else {
    line.Dec(value);
  }
  line.Char('\n');
  return io::ConsoleWriteTextActive(line.c_str());
}

}  // namespace

Status Print(std::span<const uint8_t> bytes) noexcept {
  arch::aarch64::LockGuard<arch::aarch64::IrqSpinlock> guard(g_print_lock);
  return io::ConsoleWriteActive(bytes);
}

Status PrintText(const char* text) noexcept {
  arch::aarch64::LockGuard<arch::aarch64::IrqSpinlock> guard(g_print_lock);
  return io::ConsoleWriteTextActive(text);
}

Status PrintLn(const char* text) noexcept {
  arch::aarch64::LockGuard<arch::aarch64::IrqSpinlock> guard(g_print_lock);
  char buffer[kMaxLine];
  fmt::LineBuilder line(buffer, sizeof(buffer));
  line.Text(LabelOrEmpty(text));
  line.Char('\n');
  return io::ConsoleWriteTextActive(line.c_str());
}

Status PrintDec(const char* label, uint64_t value) noexcept {
  arch::aarch64::LockGuard<arch::aarch64::IrqSpinlock> guard(g_print_lock);
  return PrintLabeled(label, value, false);
}

Status PrintHex(const char* label, uint64_t value) noexcept {
  arch::aarch64::LockGuard<arch::aarch64::IrqSpinlock> guard(g_print_lock);
  return PrintLabeled(label, value, true);
}

}  // namespace og
