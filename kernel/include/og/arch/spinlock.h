#pragma once

// Canonical owner of spin locks.
// Spin locks are a CPU primitive, so they live in arch rather than in the
// sync module, which sits above events and cannot be used there.

#include <cstdint>

#include "og/arch/cpu.h"
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
// The saved state is per CPU: a spinning core must not overwrite the state
// of the core that owns the lock.
class IrqSpinlock {
 public:
  IrqSpinlock() noexcept = default;

  IrqSpinlock(const IrqSpinlock&) = delete;
  IrqSpinlock& operator=(const IrqSpinlock&) = delete;

  void lock() noexcept {
    const uint32_t cpu = static_cast<uint32_t>(CpuId() & (kMaxCpus - 1U));
    states_[cpu] = IrqSave();
    spin_.lock();
  }

  void unlock() noexcept {
    const uint32_t cpu = static_cast<uint32_t>(CpuId() & (kMaxCpus - 1U));
    spin_.unlock();
    IrqRestore(states_[cpu]);
  }

  [[nodiscard]] bool try_lock() noexcept {
    const uint32_t cpu = static_cast<uint32_t>(CpuId() & (kMaxCpus - 1U));
    const uint64_t state = IrqSave();
    if (spin_.try_lock()) {
      states_[cpu] = state;
      return true;
    }
    IrqRestore(state);
    return false;
  }

 private:
  static constexpr uint32_t kMaxCpus = 4;
  Spinlock spin_;
  uint64_t states_[kMaxCpus]{};
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
