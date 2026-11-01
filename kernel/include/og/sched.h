#pragma once

// Canonical owner of kernel threads and scheduling.
// Threads are kernel threads at EL1 with their own stack. Each stack has a
// guard page below it.

#include <cstdint>

#include "og/status.h"

namespace og::sched {

enum class ThreadState : uint32_t {
  kReady = 0,
  kRunning = 1,
  kBlocked = 2,
  kFinished = 3,
};

struct Thread;

using ThreadEntry = void (*)(void* argument);

constexpr uint32_t kMaxThreads = 8;
constexpr uint32_t kStackPages = 4;
constexpr uint64_t kNoWake = 0xFFFFFFFFFFFFFFFFULL;

struct Thread {
  uint64_t saved_sp;
  uint64_t stack_base;
  uint64_t stack_top;
  ThreadState state;
  uint32_t id;
  uint32_t in_ready_queue;
  uint64_t wake_tick;
  char name[16];
  ThreadEntry entry;
  void* argument;
  Thread* next_ready;
  Thread* next_wait;
};

// Sleep/wake queue. A thread waits on at most one queue at a time.
struct WaitQueue {
  Thread* head;
};

[[nodiscard]] Status SchedulerInit() noexcept;
[[nodiscard]] Result<Thread*> ThreadCreate(const char* name, ThreadEntry entry,
                                           void* argument) noexcept;

// Becomes a thread and switches to the first ready thread. Returns when
// this thread is scheduled again.
void SchedulerStart() noexcept;

// Timer tick callback. Wakes sleepers and requests a reschedule.
void SchedulerTick(void* context) noexcept;

// IRQ exit hook. Performs the actual context switch after EOI.
void SchedulerIrqExit(void* context) noexcept;

[[nodiscard]] Status ThreadSleep(uint64_t ticks) noexcept;
void ThreadYield() noexcept;
[[noreturn]] void ThreadExit() noexcept;

[[nodiscard]] Thread* CurrentThread() noexcept;
[[nodiscard]] uint32_t ThreadCount() noexcept;

void WaitQueueInit(WaitQueue& queue) noexcept;
void WaitQueueWait(WaitQueue& queue) noexcept;
void WaitQueueWakeOne(WaitQueue& queue) noexcept;
void WaitQueueWakeAll(WaitQueue& queue) noexcept;

}  // namespace og::sched
