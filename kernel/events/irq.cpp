#include "og/irq.h"

#include "og/arch/registers.h"
#include "og/workqueue.h"

namespace og::events {

namespace {

struct IrqEntry {
  IrqHandler handler;
  void* context;
};

IrqEntry g_table[kMaxIrq];
InterruptController g_controller{};

}  // namespace

void SetInterruptController(InterruptController controller) noexcept {
  g_controller = controller;
}

Status RegisterIrq(uint32_t irq, IrqHandler handler, void* context) noexcept {
  if (irq >= kMaxIrq) {
    return Status::kInvalidArg;
  }
  if (handler == nullptr) {
    return Status::kInvalidArg;
  }
  const uint64_t state = arch::aarch64::IrqSave();
  Status status = Status::kOk;
  if (g_table[irq].handler != nullptr) {
    status = Status::kExists;
  } else {
    g_table[irq].handler = handler;
    g_table[irq].context = context;
  }
  arch::aarch64::IrqRestore(state);
  return status;
}

Status UnregisterIrq(uint32_t irq) noexcept {
  if (irq >= kMaxIrq) {
    return Status::kInvalidArg;
  }
  const uint64_t state = arch::aarch64::IrqSave();
  Status status = Status::kOk;
  if (g_table[irq].handler == nullptr) {
    status = Status::kNotFound;
  } else {
    g_table[irq].handler = nullptr;
    g_table[irq].context = nullptr;
  }
  arch::aarch64::IrqRestore(state);
  return status;
}

Status DispatchIrq(uint32_t irq) noexcept {
  if (irq >= kMaxIrq) {
    return Status::kInvalidArg;
  }
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
  return status;
}

}  // namespace og::events
