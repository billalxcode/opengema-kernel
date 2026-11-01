#include "og/io/tty.h"

#include "og/arch/spinlock.h"

namespace og::io::tty {

namespace {

constexpr uint32_t kBufferSize = 128;
char g_buffer[kBufferSize];
uint32_t g_head = 0;
uint32_t g_tail = 0;
uint32_t g_count = 0;
arch::aarch64::IrqSpinlock g_lock;

}  // namespace

void PushChar(char value) noexcept {
  arch::aarch64::LockGuard<arch::aarch64::IrqSpinlock> guard(g_lock);
  if (g_count >= kBufferSize) {
    return;
  }
  g_buffer[g_tail] = value;
  g_tail = (g_tail + 1U) % kBufferSize;
  ++g_count;
}

bool PopChar(char& value) noexcept {
  arch::aarch64::LockGuard<arch::aarch64::IrqSpinlock> guard(g_lock);
  if (g_count == 0U) {
    return false;
  }
  value = g_buffer[g_head];
  g_head = (g_head + 1U) % kBufferSize;
  --g_count;
  return true;
}

void Clear() noexcept {
  arch::aarch64::LockGuard<arch::aarch64::IrqSpinlock> guard(g_lock);
  g_head = 0;
  g_tail = 0;
  g_count = 0;
}

}  // namespace og::io::tty
