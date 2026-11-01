#include "og/panic.h"

#include "og/arch/registers.h"
#include "og/log.h"

namespace og {

[[noreturn]] void Panic(const char* reason) noexcept {
  arch::aarch64::DisableInterrupts();

  Log(LogLevel::kError, "panic: ");
  Log(LogLevel::kError, reason == nullptr ? "(null)" : reason);
  Log(LogLevel::kError, "\n");

  const uint64_t esr = arch::aarch64::ReadEsrEl1();
  LogHex(LogLevel::kError, "esr_el1", esr);
  LogHex(LogLevel::kError, "esr_ec", (esr >> 26U) & 0x3FU);
  LogHex(LogLevel::kError, "esr_iss", esr & 0x1FFFFFFULL);
  LogHex(LogLevel::kError, "far_el1", arch::aarch64::ReadFarEl1());
  LogHex(LogLevel::kError, "elr_el1", arch::aarch64::ReadElrEl1());
  LogHex(LogLevel::kError, "spsr_el1", arch::aarch64::ReadSpsrEl1());
  LogHex(LogLevel::kError, "vbar_el1", arch::aarch64::ReadVbarEl1());

  arch::aarch64::HaltForever();
}

}  // namespace og
