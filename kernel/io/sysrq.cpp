#include "og/io/sysrq.h"

#include "og/arch/spinlock.h"

namespace og::io {

namespace {

constexpr char kPrefix = 0x01;
bool g_pending = false;
SysRqHandler g_handler = nullptr;
arch::aarch64::IrqSpinlock g_lock;

}  // namespace

void SetSysRqHandler(SysRqHandler handler) noexcept {
  arch::aarch64::LockGuard<arch::aarch64::IrqSpinlock> guard(g_lock);
  g_handler = handler;
  g_pending = false;
}

bool SysRqFilter(char value) noexcept {
  SysRqHandler handler = nullptr;
  char key = '\0';
  {
    arch::aarch64::LockGuard<arch::aarch64::IrqSpinlock> guard(g_lock);
    if (!g_pending) {
      if (value != kPrefix) {
        return false;
      }
      g_pending = true;
      return true;
    }
    g_pending = false;
    handler = g_handler;
    key = value;
  }
  if (handler != nullptr) {
    handler(key);
  }
  return true;
}

}  // namespace og::io
