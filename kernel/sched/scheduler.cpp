#include "og/sched/sched.h"

#include "memory.h"
#include "og/arch/context.h"
#include "og/arch/cpu.h"
#include "og/arch/registers.h"
#include "og/arch/spinlock.h"
#include "og/events/irq.h"
#include "og/mem/pmm.h"
#include "og/mem/vmm.h"
#include "og/time/timer.h"

namespace og::sched {

namespace {

constexpr uint64_t kPageSize = 4096;
constexpr uint32_t kStackFrameWords = 12;  // x19..x30
// Dedicated stack region so each thread gets a real unmapped guard page
// instead of splitting the 1 GiB identity blocks.
constexpr uint64_t kStackRegionBase = 0x200000000ULL;
constexpr uint64_t kStackStride = (kStackPages + 1U) * kPageSize;
constexpr uint32_t kMaxCpus = 4;
constexpr uint32_t kAnyCpu = 0xFFFFFFFFU;

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

struct CpuScheduler {
  Thread* current;
  Thread* idle;
  Thread* ready_head;
  Thread* ready_tail;
  uint32_t ready_count;
  uint32_t need_reschedule;
  uint32_t initialized;
};

Thread g_threads[kMaxThreads];
CpuScheduler g_cpu[kMaxCpus];
arch::aarch64::IrqSpinlock g_lock;
uint32_t g_next_id = 1;
bool g_initialized = false;

[[nodiscard]] uint32_t CpuIndex() noexcept {
  const uint64_t id = arch::aarch64::CpuId();
  return id < kMaxCpus ? static_cast<uint32_t>(id) : 0U;
}

void IdleEntry(void* /*argument*/) noexcept {
  for (;;) {
    arch::aarch64::WaitForInterrupt();
  }
}

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

// Removes a thread that is still in this CPU run queue. A thread that just
// yielded or was woken while blocking can sit here while it is still the
// running context, so the scheduler must never select it for another CPU.
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

[[nodiscard]] uint32_t PickHomeLocked() noexcept {
  uint32_t best = 0;
  uint64_t best_load = 0xFFFFFFFFFFFFFFFFULL;
  for (uint32_t index = 0; index < kMaxCpus; ++index) {
    if (g_cpu[index].initialized == 0U) {
      continue;
    }
    uint64_t load = g_cpu[index].ready_count;
    if (g_cpu[index].current != nullptr && g_cpu[index].current != g_cpu[index].idle) {
      ++load;
    }
    if (load < best_load) {
      best_load = load;
      best = index;
    }
  }
  return best;
}

void Schedule() noexcept {
  const uint64_t daif = arch::aarch64::IrqSave();
  const uint32_t index = CpuIndex();
  CpuScheduler& cpu = g_cpu[index];
  g_lock.lock();

  Thread* previous = cpu.current;
  if (previous == nullptr) {
    g_lock.unlock();
    arch::aarch64::IrqRestore(daif);
    return;
  }

  // The running context may already be in the queue because it yielded or
  // was woken while blocking. Never select it here.
  (void)RemoveLocked(cpu, previous);

  Thread* next = DequeueLocked(cpu);
  while (next != nullptr && next->state != ThreadState::kReady) {
    next = DequeueLocked(cpu);
  }

  if (next == nullptr) {
    if (previous != cpu.idle && previous->state == ThreadState::kReady) {
      previous->state = ThreadState::kRunning;
      g_lock.unlock();
      arch::aarch64::IrqRestore(daif);
      return;
    }
    if (previous == cpu.idle) {
      g_lock.unlock();
      arch::aarch64::IrqRestore(daif);
      return;
    }
    next = cpu.idle;
  }

  cpu.current = next;
  next->state = ThreadState::kRunning;

  if (previous != cpu.idle && previous->state == ThreadState::kRunning) {
    previous->state = ThreadState::kReady;
    EnqueueLocked(cpu, previous);
  } else if (previous != cpu.idle && previous->state == ThreadState::kReady) {
    EnqueueLocked(cpu, previous);
  }

  g_lock.unlock();
  mem::AddressSpaceActivate(next->address_space);
  SwitchContext(&previous->saved_sp, next->saved_sp);
  arch::aarch64::IrqRestore(daif);
}

Result<Thread*> CreateThread(const char* name, ThreadEntry entry, void* argument,
                             bool enqueue, uint32_t requested_cpu,
                             bool allocate_stack,
                             mem::AddressSpace* address_space) noexcept {
  if (name == nullptr || entry == nullptr) {
    return Result<Thread*>::Err(Status::kInvalidArg);
  }

  g_lock.lock();

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
    g_lock.unlock();
    return Result<Thread*>::Err(Status::kNoMem);
  }

  const uint32_t home =
    requested_cpu == kAnyCpu ? PickHomeLocked() : requested_cpu;
  if (home >= kMaxCpus) {
    g_lock.unlock();
    return Result<Thread*>::Err(Status::kInvalidArg);
  }

  mem::Zero(slot, sizeof(*slot));
  slot->id = g_next_id;
  ++g_next_id;
  slot->home_cpu = home;

  uint64_t physical = 0;
  uint64_t virtual_base = 0;
  uint64_t top = 0;
  if (allocate_stack) {
    const Result<uint64_t> memory = mem::PmmAllocPages(kStackPages);
    if (memory.is_err()) {
      slot->id = 0;
      g_lock.unlock();
      return Result<Thread*>::Err(memory.status());
    }
    physical = memory.value();
    virtual_base = kStackRegionBase +
                   (static_cast<uint64_t>(slot_index) * kStackStride);
    top = virtual_base + kStackStride;

    // The first page of the stride stays unmapped: that is the guard page.
    for (uint32_t page = 0; page < kStackPages; ++page) {
      const uint64_t offset = static_cast<uint64_t>(page) * kPageSize;
      const Status mapped = mem::VmmMap(
        virtual_base + kPageSize + offset, physical + offset, kPageSize,
        arch::aarch64::MemoryKind::kNormal, false);
      if (IsErr(mapped)) {
        (void)mem::PmmFreePages(physical, kStackPages);
        slot->id = 0;
        g_lock.unlock();
        return Result<Thread*>::Err(mapped);
      }
    }
  }

  slot->stack_base = virtual_base;
  slot->stack_top = top;
  slot->state = ThreadState::kReady;
  slot->wake_ns = kNoWake;
  slot->entry = entry;
  slot->argument = argument;
  slot->next_ready = nullptr;
  slot->next_wait = nullptr;
  slot->wait_owner = nullptr;
  slot->address_space = address_space;
  slot->in_ready_queue = 0;

  const std::size_t length = mem::Length(name);
  const std::size_t copy =
    length >= sizeof(slot->name) ? (sizeof(slot->name) - 1U) : length;
  mem::Copy(slot->name, name, copy);
  slot->name[copy] = '\0';

  if (allocate_stack) {
    auto* context = reinterpret_cast<InitialContext*>(top - sizeof(InitialContext));
    mem::Zero(context, sizeof(*context));
    context->x19 = reinterpret_cast<uint64_t>(entry);
    context->x20 = reinterpret_cast<uint64_t>(argument);
    context->x30 = reinterpret_cast<uint64_t>(&ThreadStartTrampoline);
    slot->saved_sp = reinterpret_cast<uint64_t>(context);
  }

  if (enqueue) {
    EnqueueLocked(g_cpu[home], slot);
  }

  g_lock.unlock();
  return Result<Thread*>::Ok(slot);
}

void WakeSleepersLocked() noexcept {
  const uint64_t now = time::TimeNowNs();
  for (uint32_t index = 0; index < kMaxThreads; ++index) {
    Thread* thread = &g_threads[index];
    if (thread->id == 0U) {
      continue;
    }
    if (thread->state != ThreadState::kBlocked) {
      continue;
    }
    if (thread->wake_ns == kNoWake || now < thread->wake_ns) {
      continue;
    }
    thread->wake_ns = kNoWake;
    thread->state = ThreadState::kReady;
    EnqueueLocked(g_cpu[thread->home_cpu], thread);
  }
}

}  // namespace

Status SchedulerInit() noexcept {
  if (g_initialized) {
    return Status::kOk;
  }

  mem::Zero(g_threads, sizeof(g_threads));
  mem::Zero(g_cpu, sizeof(g_cpu));

  Thread* boot = &g_threads[0];
  boot->id = 1;
  boot->state = ThreadState::kRunning;
  boot->home_cpu = 0;
  boot->wake_ns = kNoWake;
  mem::Copy(boot->name, "boot", 5);
  g_next_id = 2;
  g_cpu[0].current = boot;
  g_cpu[0].initialized = 1;

  const Result<Thread*> idle =
    CreateThread("idle", &IdleEntry, nullptr, false, 0, true, nullptr);
  if (idle.is_err()) {
    return idle.status();
  }
  g_cpu[0].idle = idle.value();

  g_initialized = true;
  time::TimerSetTickCallback(&SchedulerTick, nullptr);
  events::SetIrqExitHook(&SchedulerIrqExit, nullptr);
  return Status::kOk;
}

Status SchedulerInitCpu() noexcept {
  if (!g_initialized) {
    return Status::kNotInit;
  }

  const uint32_t index = CpuIndex();
  g_lock.lock();
  if (g_cpu[index].initialized != 0U) {
    g_lock.unlock();
    return Status::kOk;
  }
  g_lock.unlock();

  const Result<Thread*> idle =
    CreateThread("idle", &IdleEntry, nullptr, false, index, false, nullptr);
  if (idle.is_err()) {
    return idle.status();
  }

  g_lock.lock();
  g_cpu[index].idle = idle.value();
  g_cpu[index].current = idle.value();
  g_cpu[index].need_reschedule = 0;
  g_cpu[index].initialized = 1;
  idle.value()->state = ThreadState::kRunning;
  g_lock.unlock();

  events::SetIrqExitHook(&SchedulerIrqExit, nullptr);
  return Status::kOk;
}

Result<Thread*> ThreadCreate(const char* name, ThreadEntry entry,
                             void* argument) noexcept {
  if (!g_initialized) {
    return Result<Thread*>::Err(Status::kNotInit);
  }
  return CreateThread(name, entry, argument, true, kAnyCpu, true, nullptr);
}

Result<Thread*> ThreadCreateIn(const char* name, ThreadEntry entry, void* argument,
                               mem::AddressSpace* space) noexcept {
  if (!g_initialized) {
    return Result<Thread*>::Err(Status::kNotInit);
  }
  return CreateThread(name, entry, argument, true, kAnyCpu, true, space);
}

void SchedulerStart() noexcept {
  if (!g_initialized) {
    return;
  }
  Thread* self = CurrentThread();
  if (self == nullptr) {
    return;
  }

  const uint64_t daif = arch::aarch64::IrqSave();
  g_lock.lock();
  self->state = ThreadState::kReady;
  EnqueueLocked(g_cpu[self->home_cpu], self);
  g_lock.unlock();
  Schedule();
  arch::aarch64::IrqRestore(daif);
}

void SchedulerTick(void* /*context*/) noexcept {
  const uint32_t index = CpuIndex();
  const uint64_t daif = arch::aarch64::IrqSave();
  g_lock.lock();
  WakeSleepersLocked();
  g_cpu[index].need_reschedule = 1;
  g_lock.unlock();
  arch::aarch64::IrqRestore(daif);
}

void SchedulerIrqExit(void* /*context*/) noexcept {
  const uint32_t index = CpuIndex();
  if (g_cpu[index].need_reschedule == 0U) {
    return;
  }
  g_cpu[index].need_reschedule = 0;
  Schedule();
}

Status ThreadSleep(uint64_t ticks) noexcept {
  if (!g_initialized) {
    return Status::kNotInit;
  }
  Thread* self = CurrentThread();
  if (self == nullptr || self == g_cpu[CpuIndex()].idle) {
    return Status::kInvalidArg;
  }
  if (ticks == 0U) {
    ThreadYield();
    return Status::kOk;
  }

  const uint64_t daif = arch::aarch64::IrqSave();
  g_lock.lock();
  self->state = ThreadState::kBlocked;
  self->wake_ns = time::TimeNowNs() + (ticks * kTickNanoseconds);
  g_lock.unlock();
  Schedule();
  arch::aarch64::IrqRestore(daif);
  return Status::kOk;
}

void ThreadYield() noexcept {
  if (!g_initialized) {
    return;
  }
  Thread* self = CurrentThread();
  if (self == nullptr || self == g_cpu[CpuIndex()].idle) {
    return;
  }
  const uint64_t daif = arch::aarch64::IrqSave();
  g_lock.lock();
  self->state = ThreadState::kReady;
  EnqueueLocked(g_cpu[self->home_cpu], self);
  g_lock.unlock();
  Schedule();
  arch::aarch64::IrqRestore(daif);
}

void ThreadExit() noexcept {
  arch::aarch64::DisableInterrupts();
  Thread* self = CurrentThread();
  if (self != nullptr) {
    g_lock.lock();
    (void)RemoveLocked(g_cpu[self->home_cpu], self);
    self->state = ThreadState::kFinished;
    self->wake_ns = kNoWake;
    g_lock.unlock();
  }
  for (;;) {
    Schedule();
    arch::aarch64::WaitForInterrupt();
  }
}

Thread* CurrentThread() noexcept {
  return g_cpu[CpuIndex()].current;
}

bool ThreadFinished(const Thread* thread) noexcept {
  if (thread == nullptr) {
    return false;
  }
  g_lock.lock();
  const bool finished = thread->state == ThreadState::kFinished;
  g_lock.unlock();
  return finished;
}

uint32_t ThreadCount() noexcept {
  g_lock.lock();
  uint32_t count = 0;
  for (uint32_t index = 0; index < kMaxThreads; ++index) {
    if (g_threads[index].id != 0U) {
      ++count;
    }
  }
  g_lock.unlock();
  return count;
}

void WaitQueueInit(WaitQueue& queue) noexcept {
  queue.head = nullptr;
}

void WaitQueueWait(WaitQueue& queue) noexcept {
  Thread* self = CurrentThread();
  if (self == nullptr || self == g_cpu[CpuIndex()].idle) {
    return;
  }
  const uint64_t daif = arch::aarch64::IrqSave();
  g_lock.lock();
  self->state = ThreadState::kBlocked;
  self->wake_ns = kNoWake;
  self->wait_owner = &queue;
  self->next_wait = queue.head;
  queue.head = self;
  g_lock.unlock();
  Schedule();
  arch::aarch64::IrqRestore(daif);
}

void WaitQueueWakeOne(WaitQueue& queue) noexcept {
  g_lock.lock();
  Thread* thread = queue.head;
  if (thread != nullptr) {
    queue.head = thread->next_wait;
    thread->next_wait = nullptr;
    thread->wait_owner = nullptr;
    thread->wake_ns = kNoWake;
    thread->state = ThreadState::kReady;
    EnqueueLocked(g_cpu[thread->home_cpu], thread);
  }
  g_lock.unlock();
}

void WaitQueueWakeAll(WaitQueue& queue) noexcept {
  g_lock.lock();
  Thread* thread = queue.head;
  queue.head = nullptr;
  while (thread != nullptr) {
    Thread* next = thread->next_wait;
    thread->next_wait = nullptr;
    thread->wait_owner = nullptr;
    thread->wake_ns = kNoWake;
    thread->state = ThreadState::kReady;
    EnqueueLocked(g_cpu[thread->home_cpu], thread);
    thread = next;
  }
  g_lock.unlock();
}

void WaitQueueEnqueueCurrent(WaitQueue& queue) noexcept {
  Thread* self = CurrentThread();
  if (self == nullptr || self == g_cpu[CpuIndex()].idle) {
    return;
  }
  g_lock.lock();
  if (!(self->state == ThreadState::kBlocked && self->wait_owner == &queue)) {
    self->state = ThreadState::kBlocked;
    self->wake_ns = kNoWake;
    self->wait_owner = &queue;
    self->next_wait = queue.head;
    queue.head = self;
  }
  g_lock.unlock();
}

void WakeThread(Thread* thread) noexcept {
  if (thread == nullptr) {
    return;
  }
  g_lock.lock();
  if (thread->state == ThreadState::kBlocked) {
    thread->next_wait = nullptr;
    thread->wait_owner = nullptr;
    thread->wake_ns = kNoWake;
    thread->state = ThreadState::kReady;
    EnqueueLocked(g_cpu[thread->home_cpu], thread);
  }
  g_lock.unlock();
}

void ScheduleCurrent() noexcept {
  Schedule();
}

}  // namespace og::sched

extern "C" void SchedulerThreadExit() noexcept {
  og::sched::ThreadExit();
}
