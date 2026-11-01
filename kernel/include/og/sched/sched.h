#pragma once

// Canonical owner of kernel threads and scheduling.
// Threads are kernel threads at EL1 with their own stack. Each stack has a
// guard page below it. Every CPU has its own ready queue and idle thread;
// a thread keeps the CPU it was created on.

#include <cstdint>

#include "og/core/status.h"

namespace og::mem {
struct AddressSpace;
}

namespace og::sched {

enum class ThreadState : uint32_t {
  kReady = 0,
  kRunning = 1,
  kBlocked = 2,
  kFinished = 3,
};

struct Thread;

using ThreadEntry = void (*)(void* argument);

constexpr uint32_t kMaxThreads = 16;
constexpr uint32_t kStackPages = 4;
constexpr uint64_t kNoWake = 0xFFFFFFFFFFFFFFFFULL;

// Scheduler tick rate. ThreadSleep counts these ticks.
constexpr uint64_t kTickHz = 1000;
constexpr uint64_t kTickNanoseconds = 1000000000ULL / kTickHz;

struct WaitQueue;

struct Thread {
  uint64_t saved_sp;
  uint64_t stack_base;
  uint64_t stack_top;
  ThreadState state;
  uint32_t id;
  uint32_t home_cpu;
  uint32_t in_ready_queue;
  uint64_t wake_ns;
  char name[16];
  ThreadEntry entry;
  void* argument;
  Thread* next_ready;
  Thread* next_wait;
  WaitQueue* wait_owner;
  mem::AddressSpace* address_space;
  void* process;
};

// Sleep/wake queue. A thread waits on at most one queue at a time.
struct WaitQueue {
  Thread* head;
};

// Global scheduler state. Runs on the boot CPU.
[[nodiscard]] Status SchedulerInit() noexcept;

// Per-CPU scheduler state. Runs on each secondary before it enables IRQ.
// The calling context becomes that CPU's idle thread.
[[nodiscard]] Status SchedulerInitCpu() noexcept;

[[nodiscard]] Result<Thread*> ThreadCreate(const char* name, ThreadEntry entry,
                                           void* argument) noexcept;

// Creates a thread that runs with a specific user address space installed.
[[nodiscard]] Result<Thread*> ThreadCreateIn(const char* name, ThreadEntry entry,
                                             void* argument,
                                             mem::AddressSpace* space,
                                             void* process = nullptr) noexcept;

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
[[nodiscard]] Thread* ThreadCurrentOnCpu(uint32_t cpu) noexcept;
[[nodiscard]] bool ThreadFinished(const Thread* thread) noexcept;

// Frees the kernel stack of a finished thread and returns its slot.
[[nodiscard]] Status ThreadReclaim(Thread* thread) noexcept;
[[nodiscard]] uint32_t ThreadCount() noexcept;

void WaitQueueInit(WaitQueue& queue) noexcept;
void WaitQueueWait(WaitQueue& queue) noexcept;
void WaitQueueWakeOne(WaitQueue& queue) noexcept;
void WaitQueueWakeAll(WaitQueue& queue) noexcept;

// Blocking helpers for sleeping locks. The caller must have IRQ masked.
void WaitQueueEnqueueCurrent(WaitQueue& queue) noexcept;
void WakeThread(Thread* thread) noexcept;
void ScheduleCurrent() noexcept;

}  // namespace og::sched
