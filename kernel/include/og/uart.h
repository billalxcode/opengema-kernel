#pragma once

#include <span>

#include "og/status.h"

namespace og::arch::aarch64 {

class UartPl011 {
 public:
  UartPl011() noexcept = default;
  ~UartPl011() noexcept = default;

  UartPl011(const UartPl011&) = delete;
  UartPl011& operator=(const UartPl011&) = delete;

  [[nodiscard]] Status init(uint64_t base) noexcept;
  [[nodiscard]] Status write_poll(std::span<const uint8_t> buf) noexcept;
  void putc(char c) noexcept;
  void puts(const char* s) noexcept;
  void puthex(uint64_t v) noexcept;

 private:
  uint64_t base_ = 0;
  bool is_init_ = false;
};

}  // namespace og::arch::aarch64
