#include "og/sched.h"

#include "memory.h"
#include "og/arch/context.h"
#include "og/arch/registers.h"
#include "og/arch/spinlock.h"
#include "og/irq.h"
#include "og/log.h"
#include "og/pmm.h"
#include "og/timer.h"
#include "og/vmm.h"

namespace og::sched {

namespace {

constexpr uint64_t kPageSize = 4096;
constexpr uint32_t kStackFrameWords = 12;  // x19..x30
// Dedicated stack region so each thread gets a real unmapped guard page
// instead of splitting the 1 GiB identity blocks.
constexpr uint64_t kStackRegionBase = 0x200000000ULL;
constexpr uint64_t kStackStride = (kStackPages + 1U) * kPageSize;

struct InitialContext {
  uint64_t x19;
  uint64_t x20;
  uint64_t x21;
  uint64_t x22;
  uint64_t x23;
  uint64_t x24;
  uint64_t x25;
  uint64_t x26;
  uint64_t x27;
  uint64_t x28;
  uint64_t x29;
  uint64_t x30;
};

static_assert(sizeof(InitialContext) == (kStackFrameWords * 8U));

Thread g_threads[kMaxThreads];

Thread* g_current = nullptr;
Thread* g_idle = nullptr;
Thread* g_ready_head = nullptr;
Thread* g_ready_tail = nullptr;
uint32_t g_ready_count = 0;
uint32_t g_next_id = 1;
bool g_initialized = false;
bool g_need_reschedule = false;

void IdleEntry(void* /*argument*/) noexcept {
  for (;;) {
    arch::aarch64::WaitForInterrupt();
  }
}

void EnqueueNoLock(Thread* thread) noexcept {
  if (thread == nullptr) {
    return;
  }
  if (thread->in_ready_queue != 0U) {
    return;
  }
  thread->next_ready = nullptr;
  thread->in_ready_queue = 1;
  if (g_ready_tail == nullptr) {
    g_ready_head = thread;
    g_ready_tail = thread;
  } else {
    g_ready_tail->next_ready = thread;
    g_ready_tail = thread;
  }
  ++g_ready_count;
}

Thread* DequeueNoLock() noexcept {
  Thread* thread = g_ready_head;
  if (thread == nullptr) {
    return nullptr;
  }
  g_ready_head = thread->next_ready;
  if (g_ready_head == nullptr) {
    g_ready_tail = nullptr;
  }
  thread->next_ready = nullptr;
  thread->in_ready_queue = 0;
  if (g_ready_count > 0U) {
    --g_ready_count;
  }
  return thread;
}

Thread* PickNext(Thread* previous) noexcept {
  Thread* chosen = nullptr;
  uint32_t remaining = g_ready_count;
  while (remaining > 0U) {
    Thread* candidate = DequeueNoLock();
    if (candidate == nullptr) {
      break;
    }
    --remaining;
    if (candidate == previous) {
      EnqueueNoLock(candidate);
      continue;
    }
    chosen = candidate;
    break;
  }
  if (chosen == nullptr) {
    chosen = g_idle;
  }
  return chosen;
}

void Schedule() noexcept {
  Thread* previous = g_current;
  if (previous == nullptr) {
    return;
  }

  const uint64_t daif = arch::aarch64::IrqSave();
  Thread* next = PickNext(previous);
  if (next == nullptr || next == previous) {
    arch::aarch64::IrqRestore(daif);
    return;
  }

  g_current = next;
  next->state = ThreadState::kRunning;
  SwitchContext(&previous->saved_sp, next->saved_sp);
  arch::aarch64::IrqRestore(daif);
}

Result<Thread*> CreateThread(const char* name, ThreadEntry entry, void* argument,
                             bool enqueue) noexcept {
  if (name == nullptr || entry == nullptr) {
    return Result<Thread*>::Err(Status::kInvalidArg);
  }

  Thread* slot = nullptr;
  uint32_t slot_index = 0;
  for (uint32_t index = 0; index < kMaxThreads; ++index) {
    if (g_threads[index].id == 0U) {
      slot = &g_threads[index];
      slot_index = index;
      break;
    }
  }
  if (slot == nullptr) {
    return Result<Thread*>::Err(Status::kNoMem);
  }

  const Result<uint64_t> memory = mem::PmmAllocPages(kStackPages);
  if (memory.is_err()) {
    return Result<Thread*>::Err(memory.status());
  }
  const uint64_t physical = memory.value();
  const uint64_t virtual_base = kStackRegionBase + (static_cast<uint64_t>(slot_index) * kStackStride);
  const uint64_t top = virtual_base + kStackStride;

  // The first page of the stride stays unmapped: that is the guard page.
  for (uint32_t page = 0; page < kStackPages; ++page) {
    const uint64_t offset = static_cast<uint64_t>(page) * kPageSize;
    const Status mapped = mem::VmmMap(virtual_base + kPageSize + offset, physical + offset,
                                      kPageSize, arch::aarch64::MemoryKind::kNormal, false);
    if (IsErr(mapped)) {
      (void)mem::PmmFreePages(physical, kStackPages);
      return Result<Thread*>::Err(mapped);
    }
  }

  mem::Zero(slot, sizeof(*slot));
  slot->id = g_next_id;
  ++g_next_id;
  slot->stack_base = virtual_base;
  slot->stack_top = top;
  slot->state = ThreadState::kReady;
  slot->wake_tick = kNoWake;
  slot->entry = entry;
  slot->argument = argument;
  slot->next_ready = nullptr;
  slot->next_wait = nullptr;
  slot->in_ready_queue = 0;

  const std::size_t length = mem::Length(name);
  const std::size_t copy = length >= sizeof(slot->name) ? (sizeof(slot->name) - 1U) : length;
  mem::Copy(slot->name, name, copy);
  slot->name[copy] = '\0';

  auto* context = reinterpret_cast<InitialContext*>(top - sizeof(InitialContext));
  mem::Zero(context, sizeof(*context));
  context->x19 = reinterpret_cast<uint64_t>(entry);
  context->x20 = reinterpret_cast<uint64_t>(argument);
  context->x30 = reinterpret_cast<uint64_t>(&ThreadStartTrampoline);
  slot->saved_sp = reinterpret_cast<uint64_t>(context);

  if (enqueue) {
    const uint64_t daif = arch::aarch64::IrqSave();
    EnqueueNoLock(slot);
    arch::aarch64::IrqRestore(daif);
  }

  return Result<Thread*>::Ok(slot);
}

void WakeSleepers() noexcept {
  const uint64_t now = time::TimerTicks();
  for (uint32_t index = 0; index < kMaxThreads; ++index) {
    Thread* thread = &g_threads[index];
    if (thread->id == 0U) {
      continue;
    }
    if (thread->state != ThreadState::kBlocked) {
      continue;
    }
    if (thread->wake_tick == kNoWake) {
      continue;
    }
    if (now < thread->wake_tick) {
      continue;
    }
    thread->wake_tick = kNoWake;
    thread->state = ThreadState::kReady;
    EnqueueNoLock(thread);
  }
}

}  // namespace

Status SchedulerInit() noexcept {
  if (g_initialized) {
    return Status::kOk;
  }

  mem::Zero(g_threads, sizeof(g_threads));
  Thread* boot = &g_threads[0];
  boot->id = 1;
  boot->state = ThreadState::kRunning;
  boot->wake_tick = kNoWake;
  mem::Copy(boot->name, "boot", 5);
  g_next_id = 2;
  g_current = boot;
  g_ready_head = nullptr;
  g_ready_tail = nullptr;
  g_ready_count = 0;
  g_need_reschedule = false;

  Result<Thread*> idle = CreateThread("idle", &IdleEntry, nullptr, false);
  if (idle.is_err()) {
    return idle.status();
  }
  g_idle = idle.value();

  g_initialized = true;
  time::TimerSetTickCallback(&SchedulerTick, nullptr);
  events::SetIrqExitHook(&SchedulerIrqExit, nullptr);
  return Status::kOk;
}

Result<Thread*> ThreadCreate(const char* name, ThreadEntry entry, void* argument) noexcept {
  if (!g_initialized) {
    return Result<Thread*>::Err(Status::kNotInit);
  }
  return CreateThread(name, entry, argument, true);
}

void SchedulerStart() noexcept {
  if (!g_initialized) {
    return;
  }
  Thread* self = g_current;
  if (self == nullptr) {
    return;
  }

  const uint64_t daif = arch::aarch64::IrqSave();
  self->state = ThreadState::kReady;
  EnqueueNoLock(self);
  Schedule();
  arch::aarch64::IrqRestore(daif);
}

void SchedulerTick(void* /*context*/) noexcept {
  const uint64_t daif = arch::aarch64::IrqSave();
  WakeSleepers();
  g_need_reschedule = true;
  arch::aarch64::IrqRestore(daif);
}

void SchedulerIrqExit(void* /*context*/) noexcept {
  if (!g_need_reschedule) {
    return;
  }
  g_need_reschedule = false;

  Thread* previous = g_current;
  if (previous == nullptr) {
    return;
  }
  if (previous != g_idle && previous->state == ThreadState::kRunning) {
    previous->state = ThreadState::kReady;
    EnqueueNoLock(previous);
  }
  Schedule();
}

Status ThreadSleep(uint64_t ticks) noexcept {
  if (!g_initialized) {
    return Status::kNotInit;
  }
  Thread* self = g_current;
  if (self == nullptr || self == g_idle) {
    return Status::kInvalidArg;
  }
  if (ticks == 0U) {
    ThreadYield();
    return Status::kOk;
  }

  const uint64_t daif = arch::aarch64::IrqSave();
  self->state = ThreadState::kBlocked;
  self->wake_tick = time::TimerTicks() + ticks;
  Schedule();
  arch::aarch64::IrqRestore(daif);
  return Status::kOk;
}

void ThreadYield() noexcept {
  if (!g_initialized) {
    return;
  }
  Thread* self = g_current;
  if (self == nullptr || self == g_idle) {
    return;
  }
  const uint64_t daif = arch::aarch64::IrqSave();
  self->state = ThreadState::kReady;
  EnqueueNoLock(self);
  Schedule();
  arch::aarch64::IrqRestore(daif);
}

void ThreadExit() noexcept {
  arch::aarch64::DisableInterrupts();
  Thread* self = g_current;
  if (self != nullptr) {
    self->state = ThreadState::kFinished;
  }
  for (;;) {
    Schedule();
    arch::aarch64::WaitForInterrupt();
  }
}

Thread* CurrentThread() noexcept {
  return g_current;
}

uint32_t ThreadCount() noexcept {
  uint32_t count = 0;
  for (uint32_t index = 0; index < kMaxThreads; ++index) {
    if (g_threads[index].id != 0U) {
      ++count;
    }
  }
  return count;
}

void WaitQueueInit(WaitQueue& queue) noexcept {
  queue.head = nullptr;
}

void WaitQueueWait(WaitQueue& queue) noexcept {
  Thread* self = g_current;
  if (self == nullptr || self == g_idle) {
    return;
  }
  const uint64_t daif = arch::aarch64::IrqSave();
  self->state = ThreadState::kBlocked;
  self->wake_tick = kNoWake;
  self->next_wait = queue.head;
  queue.head = self;
  Schedule();
  arch::aarch64::IrqRestore(daif);
}

void WaitQueueWakeOne(WaitQueue& queue) noexcept {
  const uint64_t daif = arch::aarch64::IrqSave();
  Thread* thread = queue.head;
  if (thread != nullptr) {
    queue.head = thread->next_wait;
    thread->next_wait = nullptr;
    thread->wake_tick = kNoWake;
    thread->state = ThreadState::kReady;
    EnqueueNoLock(thread);
  }
  arch::aarch64::IrqRestore(daif);
}

void WaitQueueWakeAll(WaitQueue& queue) noexcept {
  const uint64_t daif = arch::aarch64::IrqSave();
  Thread* thread = queue.head;
  queue.head = nullptr;
  while (thread != nullptr) {
    Thread* next = thread->next_wait;
    thread->next_wait = nullptr;
    thread->wake_tick = kNoWake;
    thread->state = ThreadState::kReady;
    EnqueueNoLock(thread);
    thread = next;
  }
  arch::aarch64::IrqRestore(daif);
}

}  // namespace og::sched

extern "C" void SchedulerThreadExit() noexcept {
  og::sched::ThreadExit();
}
