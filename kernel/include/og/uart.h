#pragma once

// PL011 UART driver. The base address is passed by the caller and lives
// in memory_map.h, never here.

#include <cstdint>
#include <span>

#include "og/status.h"

namespace og::drivers::uart {

class Pl011 {
 public:
  Pl011() noexcept = default;
  ~Pl011() noexcept = default;

  Pl011(const Pl011&) = delete;
  Pl011& operator=(const Pl011&) = delete;

  [[nodiscard]] Status init(uint64_t base) noexcept;
  [[nodiscard]] Status write_poll(std::span<const uint8_t> buffer) noexcept;

  void putc(char value) noexcept;
  void puts(const char* text) noexcept;
  void puthex(uint64_t value) noexcept;

  [[nodiscard]] bool is_ready() const noexcept { return is_init_; }
  [[nodiscard]] uint64_t base() const noexcept { return base_; }

 private:
  uint64_t base_ = 0;
  bool is_init_ = false;
};

// Routes og::Log output to this UART. Call once during early boot.
void RegisterAsLogSink(Pl011& uart) noexcept;

}  // namespace og::drivers::uart
