#pragma once

// Canonical owner of memory barriers.
// Device drivers must not emit raw dmb or dsb instructions.

namespace og::arch::aarch64 {

inline void DmbSy() noexcept {
  __asm__ volatile("dmb sy" ::: "memory");
}

inline void DsbSy() noexcept {
  __asm__ volatile("dsb sy" ::: "memory");
}

inline void DmbIsh() noexcept {
  __asm__ volatile("dmb ish" ::: "memory");
}

inline void DsbIsh() noexcept {
  __asm__ volatile("dsb ish" ::: "memory");
}

inline void DsbIshSt() noexcept {
  __asm__ volatile("dsb ishst" ::: "memory");
}

inline void DmbNsh() noexcept {
  __asm__ volatile("dmb nsh" ::: "memory");
}

inline void InstructionSync() noexcept {
  __asm__ volatile("isb" ::: "memory");
}

}  // namespace og::arch::aarch64
