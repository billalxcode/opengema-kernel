#include "og/core/trace.h"

#include "og/arch/registers.h"
#include "og/arch/spinlock.h"

namespace og::trace {

namespace {

Event g_events[kMaxEvents]{};
uint64_t g_next = 0;
arch::aarch64::IrqSpinlock g_lock;

[[nodiscard]] uint64_t NowMs() noexcept {
  const uint64_t frequency = arch::aarch64::ReadCntfrqEl0();
  if (frequency == 0U) {
    return 0U;
  }
  const uint64_t ticks = arch::aarch64::ReadCntpctEl0();
  return (ticks / frequency) * 1000U +
         (((ticks % frequency) * 1000U) / frequency);
}

}  // namespace

void Record(Tag tag, uint32_t value) noexcept {
  arch::aarch64::LockGuard<arch::aarch64::IrqSpinlock> guard(g_lock);
  Event& event = g_events[g_next % kMaxEvents];
  event.timestamp_ms = NowMs();
  event.tag = static_cast<uint32_t>(tag);
  event.value = value;
  ++g_next;
}

uint32_t Snapshot(Event* out, uint32_t capacity) noexcept {
  if (out == nullptr || capacity == 0U) {
    return 0;
  }
  arch::aarch64::LockGuard<arch::aarch64::IrqSpinlock> guard(g_lock);
  const uint32_t available =
    g_next < kMaxEvents ? static_cast<uint32_t>(g_next) : kMaxEvents;
  const uint32_t count = available < capacity ? available : capacity;
  const uint32_t first =
    g_next < kMaxEvents ? 0U : static_cast<uint32_t>(g_next - kMaxEvents);
  const uint32_t skip = available - count;
  for (uint32_t index = 0; index < count; ++index) {
    out[index] = g_events[(first + skip + index) % kMaxEvents];
  }
  return count;
}

const char* TagName(Tag tag) noexcept {
  return TagName(static_cast<uint32_t>(tag));
}

const char* TagName(uint32_t tag) noexcept {
  switch (static_cast<Tag>(tag)) {
    case Tag::kVfs:
      return "vfs";
    case Tag::kBlockRead:
      return "block_read";
    case Tag::kBlockWrite:
      return "block_write";
    case Tag::kBlockFlush:
      return "block_flush";
    case Tag::kContextSwitch:
      return "context_switch";
    case Tag::kSpawn:
      return "spawn";
    case Tag::kExit:
      return "exit";
    default:
      return "unknown";
  }
}

}  // namespace og::trace
