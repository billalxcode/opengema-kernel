#include "og/core/panic.h"

#include "og/arch/cpu.h"
#include "og/arch/registers.h"
#include "og/core/log.h"

namespace og {

namespace {

PanicInfoHook g_panic_info = nullptr;

}  // namespace

void SetPanicInfoHook(PanicInfoHook hook) noexcept {
  g_panic_info = hook;
}

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
  LogHex(LogLevel::kError, "mpidr_el1", arch::aarch64::ReadMpidrEl1());
  LogHex(LogLevel::kError, "sp", arch::aarch64::ReadSp());
  LogDec(LogLevel::kError, "current el", arch::aarch64::CurrentEl());

  if (g_panic_info != nullptr) {
    g_panic_info();
  }

  const uint64_t sp = arch::aarch64::ReadSp();
  for (uint32_t index = 0; index < 8U; ++index) {
    const auto* word = reinterpret_cast<const uint64_t*>(
      sp + (static_cast<uint64_t>(index) * sizeof(uint64_t)));
    LogHex(LogLevel::kError, "stack", *word);
  }

  arch::aarch64::HaltForever();
}

}  // namespace og
