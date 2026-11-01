#include "og/irq.h"
#include "og/log.h"
#include "og/status.h"
#include "og/workqueue.h"

namespace og::events {

namespace {

uint64_t g_irq_count = 0;
uint64_t g_work_count = 0;

Status CountingHandler(void* context, uint32_t /*irq*/) noexcept {
  if (context == nullptr) {
    return Status::kInvalidArg;
  }
  auto* counter = static_cast<uint64_t*>(context);
  ++(*counter);
  return Status::kOk;
}

void CountingWork(void* context) noexcept {
  if (context == nullptr) {
    return;
  }
  auto* counter = static_cast<uint64_t*>(context);
  ++(*counter);
}

}  // namespace

Status RunEventSelfTest() noexcept {
  constexpr uint32_t kTestIrq = 200;

  Status status = RegisterIrq(kTestIrq, &CountingHandler, &g_irq_count);
  if (IsErr(status)) {
    return status;
  }
  status = RegisterIrq(kTestIrq, &CountingHandler, &g_irq_count);
  if (status != Status::kExists) {
    (void)UnregisterIrq(kTestIrq);
    return Status::kInvalidArg;
  }

  status = DispatchIrq(kTestIrq);
  if (IsErr(status)) {
    return status;
  }
  if (g_irq_count != 1) {
    return Status::kInvalidArg;
  }

  status = UnregisterIrq(kTestIrq);
  if (IsErr(status)) {
    return status;
  }
  if (IsOk(DispatchIrq(kTestIrq))) {
    return Status::kInvalidArg;
  }

  status = ScheduleWork(&CountingWork, &g_work_count);
  if (IsErr(status)) {
    return status;
  }
  if (RunPendingWork() != 1U) {
    return Status::kInvalidArg;
  }
  if (g_work_count != 1) {
    return Status::kInvalidArg;
  }

  Log(LogLevel::kInfo, "event test: irq and workqueue ok\n");
  return Status::kOk;
}

}  // namespace og::events
