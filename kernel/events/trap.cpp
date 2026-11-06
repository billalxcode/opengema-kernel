#include "og/events/trap.h"

#include "og/arch/barriers.h"
#include "og/arch/registers.h"
#include "og/arch/trap.h"
#include "og/core/log.h"
#include "og/core/panic.h"
#include "og/events/irq.h"

// Defined in vectors.S.
extern "C" char g_vector_table[];

namespace og::events {

namespace {

[[nodiscard]] const char* VectorReason(uint64_t vector) noexcept {
  switch (vector) {
    case 0:
    case 4:
      return "trap: synchronous exception";
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

[[nodiscard]] bool IsIrqVector(uint64_t vector) noexcept {
  return vector == 1U || vector == 5U || vector == 9U;
}

[[nodiscard]] bool IsSyncVector(uint64_t vector) noexcept {
  return vector == 0U || vector == 4U || vector == 8U;
}

UserFaultHook g_user_fault = nullptr;

void DispatchTrap(arch::aarch64::TrapFrame* frame, uint64_t vector) noexcept {
  if (IsIrqVector(vector)) {
    const Status status = HandleIrq();
    if (IsErr(status)) {
      LogHex(LogLevel::kError, "trap: irq dispatch", static_cast<uint64_t>(status));
    }
    // The stub restores registers and returns with eret.
    return;
  }

  if (IsSyncVector(vector)) {
    const Status status = RunSyncHook(frame);
    if (IsOk(status)) {
      return;
    }
    if (vector == 8U && g_user_fault != nullptr) {
      const Status handled = g_user_fault(frame);
      if (IsOk(handled)) {
        return;
      }
    }
  }

  LogHex(LogLevel::kError, "trap: vector", vector);
  LogDec(LogLevel::kError, "trap: elr", frame->elr);
  Panic(VectorReason(vector));
}

}  // namespace

void InstallVectorTable() noexcept {
  arch::aarch64::WriteVbarEl1(reinterpret_cast<uint64_t>(g_vector_table));
  arch::aarch64::InstructionSync();
}

void SetUserFaultHook(UserFaultHook hook) noexcept {
  g_user_fault = hook;
}

}  // namespace og::events

extern "C" void OgTrapDispatch(og::arch::aarch64::TrapFrame* frame, uint64_t vector) noexcept {
  if (frame == nullptr) {
    og::Panic("trap: null frame");
  }
  og::events::DispatchTrap(frame, vector);
}
