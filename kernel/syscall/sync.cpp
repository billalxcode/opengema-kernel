#include "og/syscall/syscall_internal.h"

#include "og/arch/registers.h"
#include "og/arch/spinlock.h"
#include "og/mem/vmm.h"
#include "og/sched/sched.h"

namespace og::syscall {

using namespace detail;

namespace {

constexpr uint64_t kFutexWait = 0;
constexpr uint64_t kFutexWake = 1;
constexpr uint32_t kFutexBuckets = 32;
constexpr uint32_t kFutexMaxWake = 16;

sched::WaitQueue g_futex_queues[kFutexBuckets];
arch::aarch64::IrqSpinlock g_futex_lock;

[[nodiscard]] uint32_t FutexBucket(uint64_t key) noexcept {
  return static_cast<uint32_t>((key >> 2U) & (kFutexBuckets - 1U));
}

}  // namespace

Status DoFutex(uint64_t address, uint64_t operation, uint64_t value,
               uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  if ((address % sizeof(uint32_t)) != 0U ||
      !mem::VmmUserContains(process->space, address, sizeof(int32_t))) {
    return Status::kInvalidArg;
  }

  if (operation == kFutexWait) {
    const int32_t expected = static_cast<int32_t>(value);
    for (;;) {
      const uint64_t daif = arch::aarch64::IrqSave();
      g_futex_lock.lock();
      int32_t current = 0;
      const Status read =
        mem::VmmCopyIn(process->space, address, &current, sizeof(current));
      if (IsErr(read)) {
        g_futex_lock.unlock();
        arch::aarch64::IrqRestore(daif);
        return read;
      }
      if (current != expected) {
        g_futex_lock.unlock();
        arch::aarch64::IrqRestore(daif);
        result = 0;
        return Status::kOk;
      }

      const Result<uint64_t> physical =
        mem::VmmTranslateIn(process->space, address);
      if (physical.is_err()) {
        g_futex_lock.unlock();
        arch::aarch64::IrqRestore(daif);
        return physical.status();
      }
      sched::WaitQueueEnqueueCurrentWithKey(
        g_futex_queues[FutexBucket(physical.value())], physical.value());
      g_futex_lock.unlock();
      arch::aarch64::IrqRestore(daif);
      sched::ScheduleCurrent();
    }
  }

  if (operation == kFutexWake) {
    const uint32_t count =
      value > kFutexMaxWake ? kFutexMaxWake : static_cast<uint32_t>(value);
    const uint64_t daif = arch::aarch64::IrqSave();
    g_futex_lock.lock();
    const Result<uint64_t> physical =
      mem::VmmTranslateIn(process->space, address);
    if (physical.is_err()) {
      g_futex_lock.unlock();
      arch::aarch64::IrqRestore(daif);
      return physical.status();
    }
    const uint32_t woken = sched::WaitQueueWakeKey(
      g_futex_queues[FutexBucket(physical.value())], physical.value(), count);
    g_futex_lock.unlock();
    arch::aarch64::IrqRestore(daif);
    result = woken;
    return Status::kOk;
  }

  return Status::kInvalidArg;
}

}  // namespace og::syscall
