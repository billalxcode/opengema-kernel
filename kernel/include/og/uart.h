#pragma once

// PL011 UART driver. The base address is passed by the caller and lives
// in memory_map.h, never here.

#include <cstdint>
#include <span>

#include "og/console.h"
#include "og/mmio.h"
#include "og/status.h"

namespace og::drivers::uart {

// UART0 SPI on QEMU virt.
constexpr uint32_t kUartIrq = 33;
constexpr uint32_t kRingCapacity = 256;

class Pl011 {
 public:
  Pl011() noexcept = default;
  ~Pl011() noexcept = default;

  Pl011(const Pl011&) = delete;
  Pl011& operator=(const Pl011&) = delete;

  [[nodiscard]] Status init(uint64_t base, uint64_t size) noexcept;
  [[nodiscard]] Status write(std::span<const uint8_t> bytes) noexcept;

  // Configures baud, FIFO, and RX interrupt. The IRQ handler must be
  // enabled in the interrupt controller by the caller.
  [[nodiscard]] Status enable_receive(uint32_t irq, uint64_t clock_hz, uint32_t baud) noexcept;

  // Pops received bytes. Returns the number copied.
  [[nodiscard]] uint32_t read(std::span<char> out) noexcept;
  [[nodiscard]] uint32_t available() const noexcept;

  [[nodiscard]] bool is_ready() const noexcept { return ready_; }
  [[nodiscard]] uint64_t base() const noexcept { return regs_.base(); }

 private:
  [[nodiscard]] Status wait_tx_ready() noexcept;
  static Status RxHandler(void* context, uint32_t irq) noexcept;
  void push(char value) noexcept;

  io::MmioRegion regs_;
  bool ready_ = false;
  bool rx_enabled_ = false;
  char ring_[kRingCapacity]{};
  uint32_t ring_head_ = 0;
  uint32_t ring_tail_ = 0;
};

// Builds a console that forwards to this UART.
[[nodiscard]] io::Console MakeConsole(Pl011& uart) noexcept;

// Active UART, used by the kernel to enable receive interrupts later.
void SetActive(Pl011* uart) noexcept;
[[nodiscard]] Pl011* Active() noexcept;

}  // namespace og::drivers::uart
