#include "og/sync/sync.h"

#include "og/arch/registers.h"

namespace og::sync {

Status Mutex::lock() noexcept {
  const uint64_t daif = arch::aarch64::IrqSave();
  for (;;) {
    gate_.lock();
    if (!locked_) {
      locked_ = true;
      gate_.unlock();
      arch::aarch64::IrqRestore(daif);
      return Status::kOk;
    }
    sched::WaitQueueEnqueueCurrent(waiters_);
    gate_.unlock();
    sched::ScheduleCurrent();
  }
}

bool Mutex::try_lock() noexcept {
  const uint64_t daif = arch::aarch64::IrqSave();
  gate_.lock();
  const bool acquired = !locked_;
  if (acquired) {
    locked_ = true;
  }
  gate_.unlock();
  arch::aarch64::IrqRestore(daif);
  return acquired;
}

void Mutex::unlock() noexcept {
  const uint64_t daif = arch::aarch64::IrqSave();
  gate_.lock();
  locked_ = false;
  sched::Thread* waiter = waiters_.head;
  if (waiter != nullptr) {
    waiters_.head = waiter->next_wait;
    waiter->next_wait = nullptr;
    sched::WakeThread(waiter);
  }
  gate_.unlock();
  arch::aarch64::IrqRestore(daif);
}

Status Semaphore::wait() noexcept {
  const uint64_t daif = arch::aarch64::IrqSave();
  for (;;) {
    gate_.lock();
    if (count_ > 0U) {
      --count_;
      gate_.unlock();
      arch::aarch64::IrqRestore(daif);
      return Status::kOk;
    }
    sched::WaitQueueEnqueueCurrent(waiters_);
    gate_.unlock();
    sched::ScheduleCurrent();
  }
}

void Semaphore::signal() noexcept {
  const uint64_t daif = arch::aarch64::IrqSave();
  gate_.lock();
  ++count_;
  sched::Thread* waiter = waiters_.head;
  if (waiter != nullptr) {
    waiters_.head = waiter->next_wait;
    waiter->next_wait = nullptr;
    sched::WakeThread(waiter);
  }
  gate_.unlock();
  arch::aarch64::IrqRestore(daif);
}

void CallOnce(OnceFlag& flag, void (*function)(void*), void* context) noexcept {
  if (function == nullptr) {
    return;
  }

  flag.gate.lock();
  if (flag.state == 2U) {
    flag.gate.unlock();
    return;
  }
  if (flag.state == 1U) {
    flag.gate.unlock();
    while (__atomic_load_n(&flag.state, __ATOMIC_ACQUIRE) != 2U) {
    }
    return;
  }
  flag.state = 1;
  flag.gate.unlock();

  function(context);

  __atomic_store_n(&flag.state, 2U, __ATOMIC_RELEASE);
}

}  // namespace og::sync
