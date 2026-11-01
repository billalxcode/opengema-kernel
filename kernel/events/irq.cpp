#include "og/irq.h"

#include "og/arch/cpu.h"
#include "og/arch/spinlock.h"
#include "og/workqueue.h"

namespace og::events {

namespace {

struct IrqEntry {
  IrqHandler handler;
  void* context;
};

constexpr uint32_t kMaxCpuHooks = 4;

IrqEntry g_table[kMaxIrq];
InterruptController g_controller{};
arch::aarch64::IrqSpinlock g_table_lock;
IrqExitHook g_exit_hooks[kMaxCpuHooks];
void* g_exit_contexts[kMaxCpuHooks];
SyncHook g_sync_hook = nullptr;
void* g_sync_context = nullptr;

[[nodiscard]] uint32_t HookIndex() noexcept {
  return static_cast<uint32_t>(arch::aarch64::CpuId() & (kMaxCpuHooks - 1U));
}

}  // namespace

void SetInterruptController(InterruptController controller) noexcept {
  g_controller = controller;
}

void SetIrqExitHook(IrqExitHook hook, void* context) noexcept {
  const uint32_t index = HookIndex();
  g_exit_hooks[index] = hook;
  g_exit_contexts[index] = context;
}

void SetSyncHook(SyncHook hook, void* context) noexcept {
  g_sync_hook = hook;
  g_sync_context = context;
}

Status RunSyncHook(arch::aarch64::TrapFrame* frame) noexcept {
  if (g_sync_hook == nullptr) {
    return Status::kUnsupported;
  }
  return g_sync_hook(g_sync_context, frame);
}

Status RegisterIrq(uint32_t irq, IrqHandler handler, void* context) noexcept {
  if (irq >= kMaxIrq) {
    return Status::kInvalidArg;
  }
  if (handler == nullptr) {
    return Status::kInvalidArg;
  }
  arch::aarch64::LockGuard<arch::aarch64::IrqSpinlock> guard(g_table_lock);
  if (g_table[irq].handler != nullptr) {
    return Status::kExists;
  }
  g_table[irq].handler = handler;
  g_table[irq].context = context;
  return Status::kOk;
}

Status UnregisterIrq(uint32_t irq) noexcept {
  if (irq >= kMaxIrq) {
    return Status::kInvalidArg;
  }
  arch::aarch64::LockGuard<arch::aarch64::IrqSpinlock> guard(g_table_lock);
  if (g_table[irq].handler == nullptr) {
    return Status::kNotFound;
  }
  g_table[irq].handler = nullptr;
  g_table[irq].context = nullptr;
  return Status::kOk;
}

Status DispatchIrq(uint32_t irq) noexcept {
  if (irq >= kMaxIrq) {
    return Status::kInvalidArg;
  }
  // Runs with IRQ masked, so the entry cannot change under us on this core.
  const IrqHandler handler = g_table[irq].handler;
  if (handler == nullptr) {
    return Status::kNotFound;
  }
  return handler(g_table[irq].context, irq);
}

Status HandleIrq() noexcept {
  if (g_controller.acknowledge == nullptr) {
    return Status::kNotInit;
  }
  if (g_controller.end == nullptr) {
    return Status::kNotInit;
  }

  const uint32_t irq = g_controller.acknowledge(g_controller.context);
  if (irq == kSpuriousIrq) {
    return Status::kOk;
  }

  const Status status = DispatchIrq(irq);
  g_controller.end(g_controller.context, irq);
  (void)RunPendingWork();
  const uint32_t index = HookIndex();
  if (g_exit_hooks[index] != nullptr) {
    g_exit_hooks[index](g_exit_contexts[index]);
  }
  return status;
}

}  // namespace og::events
