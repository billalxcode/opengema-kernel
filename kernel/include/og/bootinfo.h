#pragma once

// Boot information passed from the UEFI loader to the kernel.
// This type is firmware neutral on purpose: nothing here mentions UEFI.

#include <cstdint>

namespace og {

enum class MemoryType : uint32_t {
  kUnknown = 0,
  kUsable = 1,
  kReserved = 2,
  kAcpiReclaim = 3,
  kAcpiNvs = 4,
  kMmio = 5,
  kRuntime = 6,
  kBootServices = 7,
};

struct MemoryRange {
  uint64_t base;
  uint64_t size;
  MemoryType type;
  uint32_t reserved;
};

static_assert(sizeof(MemoryRange) == 24, "MemoryRange layout changed");

constexpr uint32_t kBootInfoMagic = 0x4F474249U;  // "OGBI"
constexpr uint32_t kBootInfoVersion = 1U;
constexpr uint64_t kMaxMemoryRanges = 64U;

struct BootInfo {
  uint32_t magic;
  uint32_t version;
  uint64_t size;
  uint64_t memory_count;
  uint64_t uart_base;
  uint64_t ram_base;
  uint64_t ram_size;
  uint64_t image_base;
  uint64_t image_size;
  uint64_t text_base;
  uint64_t text_size;
  MemoryRange memory[kMaxMemoryRanges];
};

static_assert(sizeof(BootInfo) == 1616, "BootInfo layout changed");

}  // namespace og
