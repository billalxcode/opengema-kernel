#include "og/core/log_ring.h"

#include "og/arch/spinlock.h"

namespace og::log_ring {

namespace {

constexpr uint32_t kCapacity = 4096;

char g_buffer[kCapacity];
uint32_t g_write = 0;
uint32_t g_count = 0;
arch::aarch64::IrqSpinlock g_lock;

}  // namespace

void Append(const char* text, uint32_t length) noexcept {
  if (text == nullptr || length == 0U) {
    return;
  }
  arch::aarch64::LockGuard<arch::aarch64::IrqSpinlock> guard(g_lock);
  for (uint32_t index = 0; index < length; ++index) {
    g_buffer[g_write % kCapacity] = text[index];
    ++g_write;
    if (g_count < kCapacity) {
      ++g_count;
    }
  }
}

uint32_t Size() noexcept {
  arch::aarch64::LockGuard<arch::aarch64::IrqSpinlock> guard(g_lock);
  return g_count;
}

uint32_t Read(uint64_t offset, char* buffer, uint32_t capacity) noexcept {
  if (buffer == nullptr || capacity == 0U) {
    return 0;
  }
  arch::aarch64::LockGuard<arch::aarch64::IrqSpinlock> guard(g_lock);
  if (offset >= g_count) {
    return 0;
  }
  const uint32_t available = g_count - static_cast<uint32_t>(offset);
  const uint32_t count = available > capacity ? capacity : available;
  const uint32_t start = g_write - g_count + static_cast<uint32_t>(offset);
  for (uint32_t index = 0; index < count; ++index) {
    buffer[index] = g_buffer[(start + index) % kCapacity];
  }
  return count;
}

}  // namespace og::log_ring
