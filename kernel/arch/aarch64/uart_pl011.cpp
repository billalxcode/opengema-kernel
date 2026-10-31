#include "og/uart.h"

#include "og/memory_map.h"
#include "og/mmio.h"

namespace og::arch::aarch64 {

namespace {

constexpr uint64_t kUartDr = 0x00;
constexpr uint64_t kUartFr = 0x18;
constexpr uint32_t kUartFrTxff = 1U << 5;

}  // namespace

Status UartPl011::init(uint64_t base) noexcept {
  if (base == 0) {
    return Status::kInvalidArg;
  }
  base_ = base;
  is_init_ = true;
  return Status::kOk;
}

Status UartPl011::write_poll(std::span<const uint8_t> buf) noexcept {
  if (!is_init_) {
    return Status::kNotInit;
  }
  if (buf.empty()) {
    return Status::kInvalidArg;
  }
  if (base_ == 0) {
    return Status::kNoDevice;
  }
  for (uint8_t b : buf) {
    putc(static_cast<char>(b));
  }
  return Status::kOk;
}

void UartPl011::putc(char c) noexcept {
  if (!is_init_) {
    return;
  }
  if (base_ == 0) {
    return;
  }
  // Wait until TX FIFO not full.
  while ((MmioRead32(base_ + kUartFr) & kUartFrTxff) != 0) {
  }
  MmioWrite32(base_ + kUartDr, static_cast<uint32_t>(c));
}

void UartPl011::puts(const char* s) noexcept {
  if (!is_init_) {
    return;
  }
  if (s == nullptr) {
    return;
  }
  while (*s != '\0') {
    if (*s == '\n') {
      putc('\r');
    }
    putc(*s);
    ++s;
  }
}

void UartPl011::puthex(uint64_t v) noexcept {
  if (!is_init_) {
    return;
  }
  puts("0x");
  for (int i = 60; i >= 0; i -= 4) {
    uint32_t n = static_cast<uint32_t>((v >> i) & 0xF);
    char c = static_cast<char>(n < 10 ? '0' + n : 'a' + (n - 10));
    putc(c);
  }
}

}  // namespace og::arch::aarch64
