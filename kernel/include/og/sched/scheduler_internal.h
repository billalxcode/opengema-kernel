#pragma once

// Private scheduler entry points shared by the wait queue unit.
// Not part of the scheduler API.

#include <cstdint>

#include "og/arch/spinlock.h"
#include "og/sched/sched.h"

namespace og::sched::detail {

constexpr uint32_t kMaxCpus = 4;
constexpr uint32_t kAnyCpu = 0xFFFFFFFFU;
constexpr uint64_t kPageSize = 4096;

struct CpuScheduler {
  Thread* current;
  Thread* idle;
  Thread* ready_head;
  Thread* ready_tail;
  uint32_t ready_count;
  uint32_t need_reschedule;
  uint32_t initialized;
};

[[nodiscard]] uint32_t CpuIndex() noexcept;

[[nodiscard]] Thread* IdleThread() noexcept;

[[nodiscard]] arch::aarch64::IrqSpinlock& Lock() noexcept;

[[nodiscard]] CpuScheduler& Cpu(uint32_t index) noexcept;

// Run queue helpers. Callers must hold Lock().
void EnqueueLocked(CpuScheduler& cpu, Thread* thread) noexcept;
[[nodiscard]] Thread* DequeueLocked(CpuScheduler& cpu) noexcept;
[[nodiscard]] bool RemoveLocked(CpuScheduler& cpu, Thread* thread) noexcept;
[[nodiscard]] uint32_t PickHomeLocked() noexcept;

// Caller must hold Lock().
void EnqueueHomeLocked(Thread* thread) noexcept;

void ScheduleNow() noexcept;

[[nodiscard]] Thread* AllocateThreadLocked(uint32_t& slot_index) noexcept;

[[nodiscard]] uint32_t AllocateThreadIdLocked() noexcept;

[[nodiscard]] Result<Thread*> CreateThread(
  const char* name, ThreadEntry entry, void* argument, bool enqueue,
  uint32_t requested_cpu, bool allocate_stack,
  mem::AddressSpace* address_space, void* process) noexcept;

}  // namespace og::sched::detail
