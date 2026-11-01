#include "og/sched/scheduler_internal.h"

namespace og::sched::detail {

void EnqueueLocked(CpuScheduler& cpu, Thread* thread) noexcept {
  if (thread == nullptr || thread->in_ready_queue != 0U) {
    return;
  }
  thread->next_ready = nullptr;
  thread->in_ready_queue = 1;
  if (cpu.ready_tail == nullptr) {
    cpu.ready_head = thread;
    cpu.ready_tail = thread;
  } else {
    cpu.ready_tail->next_ready = thread;
    cpu.ready_tail = thread;
  }
  ++cpu.ready_count;
}

Thread* DequeueLocked(CpuScheduler& cpu) noexcept {
  Thread* thread = cpu.ready_head;
  if (thread == nullptr) {
    return nullptr;
  }
  cpu.ready_head = thread->next_ready;
  if (cpu.ready_head == nullptr) {
    cpu.ready_tail = nullptr;
  }
  thread->next_ready = nullptr;
  thread->in_ready_queue = 0;
  if (cpu.ready_count > 0U) {
    --cpu.ready_count;
  }
  return thread;
}

bool RemoveLocked(CpuScheduler& cpu, Thread* thread) noexcept {
  if (thread == nullptr || thread->in_ready_queue == 0U) {
    return false;
  }
  Thread** link = &cpu.ready_head;
  while (*link != nullptr) {
    if (*link == thread) {
      *link = thread->next_ready;
      if (cpu.ready_tail == thread) {
        cpu.ready_tail = nullptr;
        Thread* tail = cpu.ready_head;
        while (tail != nullptr && tail->next_ready != nullptr) {
          tail = tail->next_ready;
        }
        cpu.ready_tail = tail;
      }
      thread->next_ready = nullptr;
      thread->in_ready_queue = 0;
      if (cpu.ready_count > 0U) {
        --cpu.ready_count;
      }
      return true;
    }
    link = &(*link)->next_ready;
  }
  return false;
}

uint32_t PickHomeLocked() noexcept {
  uint32_t best = 0;
  uint64_t best_load = 0xFFFFFFFFFFFFFFFFULL;
  for (uint32_t index = 0; index < kMaxCpus; ++index) {
    CpuScheduler& cpu = Cpu(index);
    if (cpu.initialized == 0U) {
      continue;
    }
    uint64_t load = cpu.ready_count;
    if (cpu.current != nullptr && cpu.current != cpu.idle) {
      ++load;
    }
    if (load < best_load) {
      best_load = load;
      best = index;
    }
  }
  return best;
}

}  // namespace og::sched::detail
