#pragma once

// Canonical owner of AArch64 page table descriptor encoding and MAIR
// attribute indices. The virtual memory manager builds tables with these,
// the arch layer programs MAIR with the same constants.

#include <cstdint>

namespace og::arch::aarch64 {

constexpr uint64_t kPageSize = 4096;
constexpr uint64_t kBlockSize2M = 1ULL << 21U;
constexpr uint64_t kBlockSize1G = 1ULL << 30U;

constexpr uint64_t kDescriptorValid = 1ULL << 0U;
constexpr uint64_t kDescriptorTable = 3ULL << 0U;
constexpr uint64_t kDescriptorBlock = 1ULL << 0U;
constexpr uint64_t kDescriptorPage = 3ULL << 0U;
constexpr uint64_t kAttrIndexShift = 2;
constexpr uint64_t kAccessFlag = 1ULL << 10U;
constexpr uint64_t kNonGlobal = 1ULL << 11U;
constexpr uint64_t kShareInner = 3ULL << 8U;
constexpr uint64_t kPxn = 1ULL << 53U;
constexpr uint64_t kUxn = 1ULL << 54U;
constexpr uint64_t kApShift = 6;
constexpr uint64_t kApUserRw = 1ULL << kApShift;
constexpr uint64_t kApEl1Ro = 2ULL << kApShift;
// AP[1:0] = 0b11: EL1 read-only, EL0 read-only. This is the encoding user
// read-only pages need; kApEl1Ro would deny EL0 access entirely.
constexpr uint64_t kApUserRo = 3ULL << kApShift;

// MAIR attr0 is Normal write back, attr1 is Device nGnRnE.
constexpr uint64_t kMairNormalWriteBack = 0xFFULL;
constexpr uint64_t kMairDevice = 0x00ULL;
constexpr uint64_t kMairAttributeMask = 0xFFFFULL;

constexpr uint64_t kDescriptorAddressMask = 0x0000FFFFFFFFF000ULL;
// Software-only copy-on-write marker. Hardware ignores this bit; the
// kernel reads it back from the page table entry.
constexpr uint64_t kPteCow = 1ULL << 55U;

enum class MemoryKind : uint32_t {
  kNormal = 0,
  kDevice = 1,
};

[[nodiscard]] inline bool DescriptorValid(uint64_t descriptor) noexcept {
  return (descriptor & kDescriptorValid) != 0;
}

// At levels 0 to 2, bits [1:0] == 0b11 means table. At level 3 the same
// pattern means page, so callers must know the level.
[[nodiscard]] inline bool DescriptorIsTable(uint64_t descriptor) noexcept {
  return (descriptor & 3ULL) == 3ULL;
}

[[nodiscard]] inline bool DescriptorIsBlock(uint64_t descriptor) noexcept {
  return (descriptor & 3ULL) == 1ULL;
}

[[nodiscard]] inline uint64_t DescriptorAddress(uint64_t descriptor) noexcept {
  return descriptor & kDescriptorAddressMask;
}

[[nodiscard]] inline uint64_t TableDescriptor(uint64_t address) noexcept {
  return address | kDescriptorTable;
}

[[nodiscard]] inline uint64_t BlockDescriptor(uint64_t address, MemoryKind kind,
                                             bool executable, bool user = false,
                                             bool writable = true) noexcept {
  uint64_t descriptor = address | kDescriptorBlock |
                        (static_cast<uint64_t>(kind) << kAttrIndexShift) | kAccessFlag;
  if (kind == MemoryKind::kNormal) {
    descriptor |= kShareInner;
  }
  if (!executable) {
    descriptor |= kUxn | kPxn;
  }
  if (kind == MemoryKind::kDevice) {
    descriptor |= kPxn | kUxn;
  }
  if (user) {
    descriptor |= (writable ? kApUserRw : kApUserRo) | kPxn | kNonGlobal;
  }
  return descriptor;
}

[[nodiscard]] inline uint64_t PageDescriptor(uint64_t address, MemoryKind kind,
                                            bool executable, bool user = false,
                                            bool writable = true) noexcept {
  uint64_t descriptor = address | kDescriptorPage |
                        (static_cast<uint64_t>(kind) << kAttrIndexShift) | kAccessFlag;
  if (kind == MemoryKind::kNormal) {
    descriptor |= kShareInner;
  }
  if (!executable) {
    descriptor |= kUxn | kPxn;
  }
  if (kind == MemoryKind::kDevice) {
    descriptor |= kPxn | kUxn;
  }
  if (user) {
    descriptor |= (writable ? kApUserRw : kApUserRo) | kPxn | kNonGlobal;
  }
  return descriptor;
}

}  // namespace og::arch::aarch64
