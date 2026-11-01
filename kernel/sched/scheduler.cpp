#include "og/sched/sched.h"

#include "memory.h"
#include "og/arch/context.h"
#include "og/arch/cpu.h"
#include "og/arch/registers.h"
#include "og/arch/spinlock.h"
#include "og/events/irq.h"
#include "og/mem/pmm.h"
#include "og/mem/vmm.h"
#include "og/sched/scheduler_internal.h"
#include "og/time/timer.h"

namespace og::sched {

namespace {

using detail::CpuScheduler;
using detail::CreateThread;
using detail::DequeueLocked;
using detail::EnqueueLocked;
using detail::kMaxCpus;
using detail::PickHomeLocked;
using detail::RemoveLocked;

constexpr uint64_t kPageSize = 4096;
constexpr uint32_t kAnyCpu = 0xFFFFFFFFU;

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
  previous->user_sp = arch::aarch64::ReadSpEl0();

  if (previous != cpu.idle && previous->state == ThreadState::kRunning) {
    previous->state = ThreadState::kReady;
    EnqueueLocked(cpu, previous);
  } else if (previous != cpu.idle && previous->state == ThreadState::kReady) {
    EnqueueLocked(cpu, previous);
  }

  g_lock.unlock();
  mem::AddressSpaceActivate(next->address_space);
  arch::aarch64::WriteSpEl0(next->user_sp);
  SwitchContext(&previous->saved_sp, next->saved_sp);
  arch::aarch64::IrqRestore(daif);
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

namespace detail {

uint32_t CpuIndex() noexcept {
  const uint64_t id = arch::aarch64::CpuId();
  return id < kMaxCpus ? static_cast<uint32_t>(id) : 0U;
}

Thread* IdleThread() noexcept {
  return g_cpu[CpuIndex()].idle;
}

arch::aarch64::IrqSpinlock& Lock() noexcept {
  return g_lock;
}

CpuScheduler& Cpu(uint32_t index) noexcept {
  return g_cpu[index];
}

Thread* AllocateThreadLocked(uint32_t& slot_index) noexcept {
  for (uint32_t index = 0; index < kMaxThreads; ++index) {
    if (g_threads[index].id == 0U) {
      slot_index = index;
      return &g_threads[index];
    }
  }
  return nullptr;
}

uint32_t AllocateThreadIdLocked() noexcept {
  return g_next_id++;
}

void EnqueueHomeLocked(Thread* thread) noexcept {
  EnqueueLocked(g_cpu[thread->home_cpu], thread);
}

void ScheduleNow() noexcept {
  Schedule();
}

}  // namespace detail

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
    CreateThread("idle", &IdleEntry, nullptr, false, 0, true, nullptr, nullptr);
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
    CreateThread("idle", &IdleEntry, nullptr, false, index, false, nullptr, nullptr);
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
  return detail::CreateThread(name, entry, argument, true, kAnyCpu, true,
                              nullptr, nullptr);
}

Result<Thread*> ThreadCreateIn(const char* name, ThreadEntry entry, void* argument,
                               mem::AddressSpace* space, void* process) noexcept {
  if (!g_initialized) {
    return Result<Thread*>::Err(Status::kNotInit);
  }
  return detail::CreateThread(name, entry, argument, true, kAnyCpu, true, space,
                              process);
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

Thread* ThreadCurrentOnCpu(uint32_t cpu) noexcept {
  if (cpu >= kMaxCpus) {
    return nullptr;
  }
  g_lock.lock();
  Thread* current = g_cpu[cpu].current;
  g_lock.unlock();
  return current;
}

Status ThreadReclaim(Thread* thread) noexcept {
  if (thread == nullptr) {
    return Status::kInvalidArg;
  }

  g_lock.lock();
  if (thread->id == 0U) {
    g_lock.unlock();
    return Status::kNotFound;
  }
  if (thread->state != ThreadState::kFinished) {
    g_lock.unlock();
    return Status::kBusy;
  }
  if (thread->stack_base == 0U) {
    g_lock.unlock();
    return Status::kInvalidArg;
  }
  for (uint32_t cpu = 0; cpu < kMaxCpus; ++cpu) {
    if (g_cpu[cpu].current == thread) {
      g_lock.unlock();
      return Status::kBusy;
    }
  }

  if (thread->stack_base != 0U) {
    for (uint64_t address = thread->stack_base + kPageSize;
         address < thread->stack_top; address += kPageSize) {
      const Result<uint64_t> physical = mem::VmmTranslate(address);
      if (physical.is_err()) {
        continue;
      }
      const Status unmapped = mem::VmmUnmap(address, kPageSize);
      if (IsOk(unmapped)) {
        (void)mem::PmmFreePage(physical.value());
      }
    }
  }

  mem::Zero(thread, sizeof(*thread));
  g_lock.unlock();
  return Status::kOk;
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

}  // namespace og::sched

extern "C" void SchedulerThreadExit() noexcept {
  og::sched::ThreadExit();
}
