#include "og/arch/trap.h"

#include "og/arch/barriers.h"
#include "og/arch/registers.h"
#include "og/log.h"
#include "og/panic.h"

// Defined in vectors.S.
extern "C" char g_vector_table[];

namespace og::arch::aarch64 {

namespace {

[[nodiscard]] const char* VectorReason(uint64_t vector) noexcept {
  switch (vector) {
    case 0:
    case 4:
      return "trap: synchronous exception";
    case 1:
    case 5:
      return "trap: irq not handled";
    case 2:
    case 6:
      return "trap: fiq not handled";
    case 3:
    case 7:
      return "trap: serror not handled";
    default:
      return "trap: unexpected vector";
  }
}

}  // namespace

void InstallVectorTable() noexcept {
  WriteVbarEl1(reinterpret_cast<uint64_t>(g_vector_table));
  InstructionSync();
}

}  // namespace og::arch::aarch64

extern "C" void OgTrapDispatch(og::arch::aarch64::TrapFrame* frame,
                               uint64_t vector) noexcept {
  if (frame == nullptr) {
    og::Panic("trap: null frame");
  }
  og::LogHex(og::LogLevel::kError, "trap: vector", vector);
  // Panic prints ESR, FAR, ELR, SPSR, and VBAR, so no second dump here.
  og::Panic(og::arch::aarch64::VectorReason(vector));
}
