#include "og/sched/sched.h"

#include "og/arch/registers.h"
#include "og/sched/scheduler_internal.h"

namespace og::sched {

void WaitQueueInit(WaitQueue& queue) noexcept {
  queue.head = nullptr;
}

void WaitQueueWait(WaitQueue& queue) noexcept {
  Thread* self = CurrentThread();
  if (self == nullptr || self == detail::IdleThread()) {
    return;
  }
  const uint64_t daif = arch::aarch64::IrqSave();
  detail::Lock().lock();
  self->state = ThreadState::kBlocked;
  self->wake_ns = kNoWake;
  self->wait_owner = &queue;
  self->next_wait = queue.head;
  queue.head = self;
  detail::Lock().unlock();
  detail::ScheduleNow();
  arch::aarch64::IrqRestore(daif);
}

void WaitQueueWakeOne(WaitQueue& queue) noexcept {
  detail::Lock().lock();
  Thread* thread = queue.head;
  if (thread != nullptr) {
    queue.head = thread->next_wait;
    thread->next_wait = nullptr;
    thread->wait_owner = nullptr;
    thread->wake_ns = kNoWake;
    thread->state = ThreadState::kReady;
    detail::EnqueueHomeLocked(thread);
  }
  detail::Lock().unlock();
}

void WaitQueueWakeAll(WaitQueue& queue) noexcept {
  detail::Lock().lock();
  Thread* thread = queue.head;
  queue.head = nullptr;
  while (thread != nullptr) {
    Thread* next = thread->next_wait;
    thread->next_wait = nullptr;
    thread->wait_owner = nullptr;
    thread->wake_ns = kNoWake;
    thread->state = ThreadState::kReady;
    detail::EnqueueHomeLocked(thread);
    thread = next;
  }
  detail::Lock().unlock();
}

void WaitQueueEnqueueCurrent(WaitQueue& queue) noexcept {
  Thread* self = CurrentThread();
  if (self == nullptr || self == detail::IdleThread()) {
    return;
  }
  detail::Lock().lock();
  if (!(self->state == ThreadState::kBlocked && self->wait_owner == &queue)) {
    self->state = ThreadState::kBlocked;
    self->wake_ns = kNoWake;
    self->wait_owner = &queue;
    self->next_wait = queue.head;
    queue.head = self;
  }
  detail::Lock().unlock();
}

void WakeThread(Thread* thread) noexcept {
  if (thread == nullptr) {
    return;
  }
  detail::Lock().lock();
  if (thread->state == ThreadState::kBlocked) {
    thread->next_wait = nullptr;
    thread->wait_owner = nullptr;
    thread->wake_ns = kNoWake;
    thread->state = ThreadState::kReady;
    detail::EnqueueHomeLocked(thread);
  }
  detail::Lock().unlock();
}

void ScheduleCurrent() noexcept {
  detail::ScheduleNow();
}

}  // namespace og::sched
