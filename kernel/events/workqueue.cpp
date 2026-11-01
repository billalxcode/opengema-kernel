#include "og/workqueue.h"

#include "og/arch/registers.h"

namespace og::events {

namespace {

constexpr uint32_t kQueueCapacity = 64;

struct WorkItem {
  WorkFn function;
  void* context;
};

WorkItem g_queue[kQueueCapacity];
uint32_t g_head = 0;
uint32_t g_tail = 0;
uint32_t g_count = 0;
bool g_ready = false;

}  // namespace

Status WorkQueueInit() noexcept {
  g_head = 0;
  g_tail = 0;
  g_count = 0;
  g_ready = true;
  return Status::kOk;
}

Status ScheduleWork(WorkFn function, void* context) noexcept {
  if (!g_ready) {
    return Status::kNotInit;
  }
  if (function == nullptr) {
    return Status::kInvalidArg;
  }

  const uint64_t state = arch::aarch64::IrqSave();
  Status status = Status::kOk;
  if (g_count == kQueueCapacity) {
    status = Status::kNoMem;
  } else {
    g_queue[g_tail].function = function;
    g_queue[g_tail].context = context;
    g_tail = (g_tail + 1U) % kQueueCapacity;
    ++g_count;
  }
  arch::aarch64::IrqRestore(state);
  return status;
}

uint32_t RunPendingWork() noexcept {
  if (!g_ready) {
    return 0;
  }
  uint32_t ran = 0;
  for (;;) {
    WorkItem item{};
    const uint64_t state = arch::aarch64::IrqSave();
    if (g_count == 0) {
      arch::aarch64::IrqRestore(state);
      break;
    }
    item = g_queue[g_head];
    g_head = (g_head + 1U) % kQueueCapacity;
    --g_count;
    arch::aarch64::IrqRestore(state);
    item.function(item.context);
    ++ran;
  }
  return ran;
}

uint32_t PendingWork() noexcept {
  return g_count;
}

}  // namespace og::events
