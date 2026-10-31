#include "og/uart.h"

#include "og/log.h"
#include "og/mmio.h"

namespace og::drivers::uart {

namespace {

constexpr uint64_t kDataRegister = 0x00;
constexpr uint64_t kFlagRegister = 0x18;
constexpr uint32_t kTxFifoFull = 1U << 5;

void LogSinkAdapter(void* context, LogLevel /*level*/, const char* text) noexcept {
  if (context == nullptr) {
    return;
  }
  if (text == nullptr) {
    return;
  }
  static_cast<Pl011*>(context)->puts(text);
}

}  // namespace

Status Pl011::init(uint64_t base) noexcept {
  if (base == 0) {
    return Status::kInvalidArg;
  }
  base_ = base;
  is_init_ = true;
  return Status::kOk;
}

Status Pl011::write_poll(std::span<const uint8_t> buffer) noexcept {
  if (!is_init_) {
    return Status::kNotInit;
  }
  if (buffer.empty()) {
    return Status::kInvalidArg;
  }
  for (uint8_t byte : buffer) {
    putc(static_cast<char>(byte));
  }
  return Status::kOk;
}

void Pl011::putc(char value) noexcept {
  if (!is_init_) {
    return;
  }
  // Wait until the TX FIFO has room. Bounded to avoid an infinite loop if
  // the device is misconfigured.
  for (uint32_t spin = 0; spin < 100000U; ++spin) {
    if ((io::MmioRead32(base_ + kFlagRegister) & kTxFifoFull) == 0U) {
      io::MmioWrite32(base_ + kDataRegister, static_cast<uint32_t>(value));
      return;
    }
  }
}

void Pl011::puts(const char* text) noexcept {
  if (!is_init_) {
    return;
  }
  if (text == nullptr) {
    return;
  }
  while (*text != '\0') {
    if (*text == '\n') {
      putc('\r');
    }
    putc(*text);
    ++text;
  }
}

void Pl011::puthex(uint64_t value) noexcept {
  if (!is_init_) {
    return;
  }
  puts("0x");
  for (int shift = 60; shift >= 0; shift -= 4) {
    const uint32_t nibble = static_cast<uint32_t>((value >> shift) & 0xFU);
    const char digit = static_cast<char>(nibble < 10U ? '0' + nibble : 'a' + (nibble - 10U));
    putc(digit);
  }
}

void RegisterAsLogSink(Pl011& uart) noexcept {
  if (!uart.is_ready()) {
    return;
  }
  SetLogSink(&LogSinkAdapter, &uart);
}

}  // namespace og::drivers::uart
