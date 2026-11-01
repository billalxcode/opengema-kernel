#pragma once

// Canonical owner of sleeping locks and reference counting.
// These primitives may block, so they live above the scheduler.

#include <cstdint>

#include "og/arch/spinlock.h"
#include "og/core/status.h"
#include "og/sched/sched.h"

namespace og::sync {

class Mutex {
 public:
  constexpr Mutex() noexcept = default;

  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

  void set_name(const char* name) noexcept { name_ = name; }
  [[nodiscard]] const char* name() const noexcept { return name_; }

  [[nodiscard]] Status lock() noexcept;
  [[nodiscard]] bool try_lock() noexcept;
  void unlock() noexcept;

 private:
  arch::aarch64::Spinlock gate_;
  sched::WaitQueue waiters_{};
  bool locked_ = false;
  const char* name_ = nullptr;
};

class Semaphore {
 public:
  Semaphore() noexcept = default;

  Semaphore(const Semaphore&) = delete;
  Semaphore& operator=(const Semaphore&) = delete;

  void init(uint32_t count) noexcept { count_ = count; }

  [[nodiscard]] Status wait() noexcept;
  void signal() noexcept;
  [[nodiscard]] uint32_t count() const noexcept { return count_; }

 private:
  arch::aarch64::Spinlock gate_;
  sched::WaitQueue waiters_{};
  uint32_t count_ = 0;
};

class RefCount {
 public:
  constexpr RefCount() noexcept = default;

  void reset(uint32_t value = 1) noexcept {
    __atomic_store_n(&value_, value, __ATOMIC_RELEASE);
  }

  [[nodiscard]] uint32_t add(uint32_t count = 1) noexcept {
    return __atomic_add_fetch(&value_, count, __ATOMIC_RELAXED);
  }

  // Returns true when this release dropped the count to zero.
  [[nodiscard]] bool release() noexcept {
    return __atomic_sub_fetch(&value_, 1U, __ATOMIC_ACQ_REL) == 0U;
  }

  [[nodiscard]] uint32_t value() const noexcept {
    return __atomic_load_n(&value_, __ATOMIC_ACQUIRE);
  }

 private:
  uint32_t value_ = 1;
};

struct OnceFlag {
  arch::aarch64::Spinlock gate;
  uint32_t state = 0;  // 0 idle, 1 running, 2 done
};

void CallOnce(OnceFlag& flag, void (*function)(void*), void* context) noexcept;

}  // namespace og::sync
