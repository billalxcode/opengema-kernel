#pragma once

// Canonical owner of AArch64 system register access.
// Nothing else may read or write these registers with inline asm.

#include <cstdint>

namespace og::arch::aarch64 {

[[nodiscard]] inline uint64_t ReadEsrEl1() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, esr_el1" : "=r"(value));
  return value;
}

[[nodiscard]] inline uint64_t ReadFarEl1() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, far_el1" : "=r"(value));
  return value;
}

[[nodiscard]] inline uint64_t ReadElrEl1() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, elr_el1" : "=r"(value));
  return value;
}

[[nodiscard]] inline uint64_t ReadSpsrEl1() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, spsr_el1" : "=r"(value));
  return value;
}

[[nodiscard]] inline uint64_t ReadVbarEl1() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, vbar_el1" : "=r"(value));
  return value;
}

inline void WriteVbarEl1(uint64_t value) noexcept {
  __asm__ volatile("msr vbar_el1, %0" ::"r"(value) : "memory");
}

[[nodiscard]] inline uint64_t ReadSctlrEl1() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, sctlr_el1" : "=r"(value));
  return value;
}

inline void WriteSctlrEl1(uint64_t value) noexcept {
  __asm__ volatile("msr sctlr_el1, %0" ::"r"(value) : "memory");
}

[[nodiscard]] inline uint64_t ReadTcrEl1() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, tcr_el1" : "=r"(value));
  return value;
}

inline void WriteTcrEl1(uint64_t value) noexcept {
  __asm__ volatile("msr tcr_el1, %0" ::"r"(value) : "memory");
}

[[nodiscard]] inline uint64_t ReadTtbr0El1() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, ttbr0_el1" : "=r"(value));
  return value;
}

inline void WriteTtbr0El1(uint64_t value) noexcept {
  __asm__ volatile("msr ttbr0_el1, %0" ::"r"(value) : "memory");
}

[[nodiscard]] inline uint64_t ReadIdAa64Mmfr0El1() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, id_aa64mmfr0_el1" : "=r"(value));
  return value;
}

[[nodiscard]] inline uint64_t ReadIdAa64Mmfr1El1() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, id_aa64mmfr1_el1" : "=r"(value));
  return value;
}

[[nodiscard]] inline uint64_t ReadMairEl1() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, mair_el1" : "=r"(value));
  return value;
}

inline void WriteMairEl1(uint64_t value) noexcept {
  __asm__ volatile("msr mair_el1, %0" ::"r"(value) : "memory");
}

[[nodiscard]] inline uint64_t ReadDaif() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, daif" : "=r"(value));
  return value;
}

inline void WriteDaif(uint64_t value) noexcept {
  __asm__ volatile("msr daif, %0" ::"r"(value) : "memory");
}

inline void DisableInterrupts() noexcept {
  __asm__ volatile("msr daifset, #0xf" ::: "memory");
}

inline void EnableInterrupts() noexcept {
  __asm__ volatile("msr daifclr, #0xf" ::: "memory");
}

inline void DisableIrq() noexcept {
  __asm__ volatile("msr daifset, #2" ::: "memory");
}

inline void EnableIrq() noexcept {
  __asm__ volatile("msr daifclr, #2" ::: "memory");
}

// Saves DAIF and masks IRQ. Restore with IrqRestore.
[[nodiscard]] inline uint64_t IrqSave() noexcept {
  const uint64_t state = ReadDaif();
  DisableIrq();
  return state;
}

inline void IrqRestore(uint64_t state) noexcept {
  WriteDaif(state);
}

inline void WaitForInterrupt() noexcept {
  __asm__ volatile("wfi");
}

[[noreturn]] inline void HaltForever() noexcept {
  for (;;) {
    WaitForInterrupt();
  }
}

// Generic timer, EL1 physical counter.
[[nodiscard]] inline uint64_t ReadCntfrqEl0() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(value));
  return value;
}

[[nodiscard]] inline uint64_t ReadCntpctEl0() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, cntpct_el0" : "=r"(value));
  return value;
}

inline void WriteCntpTvalEl0(uint64_t value) noexcept {
  __asm__ volatile("msr cntp_tval_el0, %0" ::"r"(value) : "memory");
}

[[nodiscard]] inline uint64_t ReadCntpCtlEl0() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, cntp_ctl_el0" : "=r"(value));
  return value;
}

inline void WriteCntpCtlEl0(uint64_t value) noexcept {
  __asm__ volatile("msr cntp_ctl_el0, %0" ::"r"(value) : "memory");
}

// GICv3 system register interface.
[[nodiscard]] inline uint64_t ReadIccSreEl1() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, s3_0_c12_c12_5" : "=r"(value));
  return value;
}

inline void WriteIccSreEl1(uint64_t value) noexcept {
  __asm__ volatile("msr s3_0_c12_c12_5, %0" ::"r"(value) : "memory");
}

[[nodiscard]] inline uint64_t ReadIccCtlrEl1() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, s3_0_c12_c12_4" : "=r"(value));
  return value;
}

inline void WriteIccCtlrEl1(uint64_t value) noexcept {
  __asm__ volatile("msr s3_0_c12_c12_4, %0" ::"r"(value) : "memory");
}

inline void WriteIccPmrEl1(uint64_t value) noexcept {
  __asm__ volatile("msr s3_0_c4_c6_0, %0" ::"r"(value) : "memory");
}

inline void WriteIccIgrpen1El1(uint64_t value) noexcept {
  __asm__ volatile("msr s3_0_c12_c12_7, %0" ::"r"(value) : "memory");
}

[[nodiscard]] inline uint64_t ReadIccIar1El1() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, s3_0_c12_c12_0" : "=r"(value));
  return value;
}

inline void WriteIccEoir1El1(uint64_t value) noexcept {
  __asm__ volatile("msr s3_0_c12_c12_1, %0" ::"r"(value) : "memory");
}

inline void WriteIccSgi1rEl1(uint64_t value) noexcept {
  __asm__ volatile("msr s3_0_c12_c11_5, %0" ::"r"(value) : "memory");
  __asm__ volatile("isb" ::: "memory");
}

[[nodiscard]] inline uint64_t ReadTpidrEl1() noexcept {  uint64_t value = 0;
  __asm__ volatile("mrs %0, tpidr_el1" : "=r"(value));
  return value;
}

inline void WriteTpidrEl1(uint64_t value) noexcept {
  __asm__ volatile("msr tpidr_el1, %0" ::"r"(value) : "memory");
}

[[nodiscard]] inline uint64_t ReadSp() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mov %0, sp" : "=r"(value));
  return value;
}

[[nodiscard]] inline uint64_t ReadSpEl0() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, sp_el0" : "=r"(value));
  return value;
}

inline void WriteSpEl0(uint64_t value) noexcept {
  __asm__ volatile("msr sp_el0, %0" ::"r"(value) : "memory");
}

[[nodiscard]] inline uint64_t ReadTpidrEl0() noexcept {
  uint64_t value = 0;
  __asm__ volatile("mrs %0, tpidr_el0" : "=r"(value));
  return value;
}

inline void WriteTpidrEl0(uint64_t value) noexcept {
  __asm__ volatile("msr tpidr_el0, %0" ::"r"(value) : "memory");
}

}  // namespace og::arch::aarch64
