#pragma once

// PL011 UART driver. The base address is passed by the caller and lives
// in memory_map.h, never here.

#include <cstdint>
#include <span>

#include "og/console.h"
#include "og/mmio.h"
#include "og/status.h"

namespace og::drivers::uart {

class Pl011 {
 public:
  Pl011() noexcept = default;
  ~Pl011() noexcept = default;

  Pl011(const Pl011&) = delete;
  Pl011& operator=(const Pl011&) = delete;

  [[nodiscard]] Status init(uint64_t base, uint64_t size) noexcept;
  [[nodiscard]] Status write(std::span<const uint8_t> bytes) noexcept;

  [[nodiscard]] bool is_ready() const noexcept { return ready_; }
  [[nodiscard]] uint64_t base() const noexcept { return regs_.base(); }

 private:
  [[nodiscard]] Status wait_tx_ready() noexcept;

  io::MmioRegion regs_;
  bool ready_ = false;
};

// Builds a console that forwards to this UART.
[[nodiscard]] io::Console MakeConsole(Pl011& uart) noexcept;

}  // namespace og::drivers::uart
