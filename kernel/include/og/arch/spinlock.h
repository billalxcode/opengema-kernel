#pragma once

// Canonical owner of spin locks.
// Spin locks are a CPU primitive, so they live in arch rather than in the
// sync module, which sits above events and cannot be used there.

#include <cstdint>

#include "og/arch/registers.h"

namespace og::arch::aarch64 {

class Spinlock {
 public:
  constexpr Spinlock() noexcept = default;

  Spinlock(const Spinlock&) = delete;
  Spinlock& operator=(const Spinlock&) = delete;

  void lock() noexcept {
    while (__atomic_exchange_n(&locked_, 1U, __ATOMIC_ACQUIRE) != 0U) {
    }
  }

  void unlock() noexcept {
    __atomic_store_n(&locked_, 0U, __ATOMIC_RELEASE);
  }

  [[nodiscard]] bool try_lock() noexcept {
    return __atomic_exchange_n(&locked_, 1U, __ATOMIC_ACQUIRE) == 0U;
  }

 private:
  uint32_t locked_ = 0;
};

// Saves DAIF, masks IRQ, then takes the spin lock. Unlock restores DAIF,
// so a lock held across an interrupt stays safe on a single core.
class IrqSpinlock {
 public:
  IrqSpinlock() noexcept = default;

  IrqSpinlock(const IrqSpinlock&) = delete;
  IrqSpinlock& operator=(const IrqSpinlock&) = delete;

  void lock() noexcept {
    state_ = IrqSave();
    spin_.lock();
  }

  void unlock() noexcept {
    spin_.unlock();
    IrqRestore(state_);
  }

 private:
  Spinlock spin_;
  uint64_t state_ = 0;
};

template <typename Lock>
class LockGuard {
 public:
  explicit LockGuard(Lock& lock) noexcept : lock_(lock) {
    lock_.lock();
  }

  ~LockGuard() noexcept {
    lock_.unlock();
  }

  LockGuard(const LockGuard&) = delete;
  LockGuard& operator=(const LockGuard&) = delete;

 private:
  Lock& lock_;
};

}  // namespace og::arch::aarch64
