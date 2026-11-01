#pragma once

// Canonical owner of interrupt registration and dispatch.
// The interrupt controller is supplied by a driver, so this layer stays
// independent of GIC specifics.

#include <cstdint>

#include "og/arch/trap.h"
#include "og/status.h"

namespace og::events {

constexpr uint32_t kMaxIrq = 256;
constexpr uint32_t kSpuriousIrq = 1023;

using IrqHandler = Status (*)(void* context, uint32_t irq);

struct InterruptController {
  uint32_t (*acknowledge)(void* context);
  void (*end)(void* context, uint32_t irq);
  void* context;
};

// Must be called before interrupts are enabled.
void SetInterruptController(InterruptController controller) noexcept;

// Runs after EOI and after pending work, in IRQ context. Used by the
// scheduler to switch threads only after the interrupt is finished.
using IrqExitHook = void (*)(void* context) noexcept;
void SetIrqExitHook(IrqExitHook hook, void* context) noexcept;

// Synchronous exception hook. The syscall layer registers here. Returning
// kOk means the exception was handled and the trap can return.
using SyncHook = Status (*)(void* context, arch::aarch64::TrapFrame* frame) noexcept;
void SetSyncHook(SyncHook hook, void* context) noexcept;
[[nodiscard]] Status RunSyncHook(arch::aarch64::TrapFrame* frame) noexcept;

[[nodiscard]] Status RegisterIrq(uint32_t irq, IrqHandler handler, void* context) noexcept;
[[nodiscard]] Status UnregisterIrq(uint32_t irq) noexcept;

// Calls the handler registered for irq. Safe from IRQ context.
[[nodiscard]] Status DispatchIrq(uint32_t irq) noexcept;

// Acknowledge, dispatch, end, then run pending work. Called from the trap
// entry for IRQ vectors.
[[nodiscard]] Status HandleIrq() noexcept;

}  // namespace og::events
