#include "og/sched/scheduler_internal.h"

#include "memory.h"
#include "og/arch/context.h"
#include "og/arch/descriptor.h"
#include "og/mem/pmm.h"
#include "og/mem/vmm.h"

namespace og::sched::detail {

namespace {

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

}  // namespace

Result<Thread*> CreateThread(const char* name, ThreadEntry entry, void* argument,
                             bool enqueue, uint32_t requested_cpu,
                             bool allocate_stack,
                             mem::AddressSpace* address_space,
                             void* process) noexcept {
  if (name == nullptr || entry == nullptr) {
    return Result<Thread*>::Err(Status::kInvalidArg);
  }

  Lock().lock();

  uint32_t slot_index = 0;
  Thread* slot = AllocateThreadLocked(slot_index);
  if (slot == nullptr) {
    Lock().unlock();
    return Result<Thread*>::Err(Status::kNoMem);
  }

  const uint32_t home =
    requested_cpu == kAnyCpu ? PickHomeLocked() : requested_cpu;
  if (home >= kMaxCpus) {
    Lock().unlock();
    return Result<Thread*>::Err(Status::kInvalidArg);
  }

  mem::Zero(slot, sizeof(*slot));
  slot->id = AllocateThreadIdLocked();
  slot->home_cpu = home;

  uint64_t physical = 0;
  uint64_t virtual_base = 0;
  uint64_t top = 0;
  if (allocate_stack) {
    const Result<uint64_t> memory = mem::PmmAllocPages(kStackPages);
    if (memory.is_err()) {
      slot->id = 0;
      Lock().unlock();
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
        Lock().unlock();
        return Result<Thread*>::Err(mapped);
      }
    }
  }

  slot->stack_base = virtual_base;
  slot->stack_top = top;
  slot->state = ThreadState::kReady;
  slot->wake_ns = kNoWake;
  slot->base_priority = kPriorityDefault;
  slot->wait_ticks = 0;
  slot->entry = entry;
  slot->argument = argument;
  slot->next_ready = nullptr;
  slot->next_wait = nullptr;
  slot->wait_owner = nullptr;
  slot->address_space = address_space;
  slot->process = process;
  slot->in_ready_queue = 0;

  const std::size_t length = mem::Length(name);
  const std::size_t copy =
    length >= sizeof(slot->name) ? (sizeof(slot->name) - 1U) : length;
  mem::Copy(slot->name, name, copy);
  slot->name[copy] = '\0';

  if (allocate_stack) {
    auto* context =
      reinterpret_cast<InitialContext*>(top - sizeof(InitialContext));
    mem::Zero(context, sizeof(*context));
    context->x19 = reinterpret_cast<uint64_t>(entry);
    context->x20 = reinterpret_cast<uint64_t>(argument);
    context->x30 = reinterpret_cast<uint64_t>(&ThreadStartTrampoline);
    slot->saved_sp = reinterpret_cast<uint64_t>(context);
  }

  if (enqueue) {
    EnqueueLocked(Cpu(home), slot);
  }

  Lock().unlock();
  return Result<Thread*>::Ok(slot);
}

}  // namespace og::sched::detail
