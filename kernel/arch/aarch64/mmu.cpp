#include "og/mmu.h"

#include "memory.h"
#include "og/arch/barriers.h"
#include "og/arch/cache.h"
#include "og/arch/registers.h"
#include "og/arch/tlb.h"

namespace og::arch::aarch64 {

namespace {

constexpr uint64_t kPageSize = 4096;
constexpr uint64_t kBlockSize1G = 1ULL << 30U;
constexpr uint64_t kTableEntries = 512;

constexpr uint64_t kDescriptorTable = 3ULL << 0U;
constexpr uint64_t kDescriptorBlock = 1ULL << 0U;
constexpr uint64_t kAttrIndexShift = 2;
constexpr uint64_t kAccessFlag = 1ULL << 10U;
constexpr uint64_t kShareInner = 3ULL << 8U;
constexpr uint64_t kPxn = 1ULL << 53U;
constexpr uint64_t kUxn = 1ULL << 54U;

// MAIR attr0: Normal write back, inner and outer. attr1: Device nGnRnE.
constexpr uint64_t kMairNormalWriteBack = 0xFFULL;
constexpr uint64_t kMairDevice = 0x00ULL;

// TCR: 48 bit VA, 4 KiB granule, write back walk, inner shareable.
constexpr uint64_t kTcrT0Sz48 = 16;
constexpr uint64_t kTcrIrgn0Shift = 8;
constexpr uint64_t kTcrOrgn0Shift = 10;
constexpr uint64_t kTcrSh0Shift = 12;
constexpr uint64_t kTcrTg0Shift = 14;
constexpr uint64_t kTcrAttributeMask =
  (0x3FULL << 0U) | (0x3ULL << kTcrIrgn0Shift) | (0x3ULL << kTcrOrgn0Shift) |
  (0x3ULL << kTcrSh0Shift) | (0x1ULL << kTcrTg0Shift);

constexpr uint64_t kSctlrMmuEnable = 1ULL << 0U;
constexpr uint64_t kSctlrDataCacheEnable = 1ULL << 2U;
constexpr uint64_t kSctlrInstructionCacheEnable = 1ULL << 12U;

alignas(kPageSize) uint64_t g_level0[kTableEntries];
alignas(kPageSize) uint64_t g_level1[kTableEntries];

[[nodiscard]] uint64_t NormalBlock(uint64_t address) noexcept {
  return address | kDescriptorBlock | (0ULL << kAttrIndexShift) | kAccessFlag | kShareInner | kUxn;
}

[[nodiscard]] uint64_t DeviceBlock(uint64_t address) noexcept {
  return address | kDescriptorBlock | (1ULL << kAttrIndexShift) | kAccessFlag | kPxn | kUxn;
}

}  // namespace

Status Mmu::init_identity(uint64_t ram_base, uint64_t ram_size) noexcept {
  if (ram_size == 0) {
    return Status::kInvalidArg;
  }
  if ((ram_base % kBlockSize1G) != 0) {
    return Status::kUnaligned;
  }

  const uint64_t first_index = ram_base / kBlockSize1G;
  const uint64_t blocks = (ram_size + kBlockSize1G - 1U) / kBlockSize1G;
  if (first_index >= kTableEntries) {
    return Status::kInvalidArg;
  }
  if (blocks > (kTableEntries - first_index)) {
    return Status::kInvalidArg;
  }

  mem::Zero(g_level0, sizeof(g_level0));
  mem::Zero(g_level1, sizeof(g_level1));

  g_level0[0] = reinterpret_cast<uint64_t>(g_level1) | kDescriptorTable;

  for (uint64_t index = 0; index < kTableEntries; ++index) {
    const uint64_t address = index * kBlockSize1G;
    if (index >= first_index && index < (first_index + blocks)) {
      g_level1[index] = NormalBlock(address);
    } else if (index < first_index) {
      g_level1[index] = DeviceBlock(address);
    }
  }

  // Page table writes must be visible to the walker before TTBR0 changes.
  CleanInvalidateDcacheRange(reinterpret_cast<uint64_t>(g_level0),
                             reinterpret_cast<uint64_t>(g_level1) + sizeof(g_level1));

  uint64_t mair = ReadMairEl1();
  mair = (mair & ~0xFFFFULL) | kMairNormalWriteBack | (kMairDevice << 8U);
  WriteMairEl1(mair);

  uint64_t tcr = ReadTcrEl1();
  tcr &= ~kTcrAttributeMask;
  tcr |= kTcrT0Sz48;
  tcr |= 1ULL << kTcrIrgn0Shift;
  tcr |= 1ULL << kTcrOrgn0Shift;
  tcr |= 3ULL << kTcrSh0Shift;
  // Tg0 = 0 selects 4 KiB granule.
  WriteTcrEl1(tcr);

  DsbSy();
  WriteTtbr0El1(reinterpret_cast<uint64_t>(g_level0));
  DsbSy();
  InstructionSync();
  InvalidateTlbAllLocal();

  uint64_t sctlr = ReadSctlrEl1();
  sctlr |= kSctlrMmuEnable | kSctlrDataCacheEnable | kSctlrInstructionCacheEnable;
  WriteSctlrEl1(sctlr);
  InstructionSync();
  InvalidateIcacheAll();

  enabled_ = true;
  return Status::kOk;
}

}  // namespace og::arch::aarch64
