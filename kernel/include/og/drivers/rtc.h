#pragma once

// PL031 real time clock. Provides Unix seconds for log timestamps.

#include <cstdint>

#include "og/core/status.h"
#include "og/io/mmio.h"

namespace og::drivers::rtc {

class Pl031 {
 public:
  Pl031() noexcept = default;
  ~Pl031() noexcept = default;

  Pl031(const Pl031&) = delete;
  Pl031& operator=(const Pl031&) = delete;

  [[nodiscard]] Status init(uint64_t base, uint64_t size) noexcept;

  [[nodiscard]] bool is_ready() const noexcept { return ready_; }
  [[nodiscard]] Result<uint64_t> read_unix_seconds() noexcept;
  [[nodiscard]] Status write_unix_seconds(uint64_t seconds) noexcept;

 private:
  io::MmioRegion registers_;
  bool ready_ = false;
};

}  // namespace og::drivers::rtc
