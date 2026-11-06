#include "og/arch/mmu.h"

#include "og/arch/barriers.h"
#include "og/arch/cache.h"
#include "og/arch/descriptor.h"
#include "og/arch/registers.h"
#include "og/arch/tlb.h"

namespace og::arch::aarch64 {

namespace {

// TCR: 48 bit VA, 4 KiB granule, write back walks, inner shareable.
// IPS is derived from PARange so secondary cores do not inherit a 32 bit
// physical address limit from firmware.
constexpr uint64_t kTcrT0Sz48 = 16;
constexpr uint64_t kTcrIpsShift = 32;
constexpr uint64_t kTcrIpsMask = 0x7ULL << kTcrIpsShift;
constexpr uint64_t kTcrMaxIps = 5;
constexpr uint64_t kTcrA1 = 1ULL << 22U;
constexpr unsigned kTtbrAsidShift = 48U;
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

}  // namespace

Status ActivateAddressSpace(uint64_t ttbr0) noexcept {
  if (ttbr0 == 0) {
    return Status::kInvalidArg;
  }
  if ((ttbr0 % kPageSize) != 0) {
    return Status::kUnaligned;
  }

  uint64_t mair = ReadMairEl1();
  mair = (mair & ~kMairAttributeMask) | kMairNormalWriteBack | (kMairDevice << 8U);
  WriteMairEl1(mair);

  const uint64_t parange = ReadIdAa64Mmfr0El1() & 0xFULL;
  const uint64_t ips = parange > kTcrMaxIps ? kTcrMaxIps : parange;

  uint64_t tcr = ReadTcrEl1();
  tcr &= ~kTcrAttributeMask;
  tcr &= ~kTcrIpsMask;
  tcr &= ~kTcrA1;
  tcr |= ips << kTcrIpsShift;
  tcr |= kTcrT0Sz48;
  tcr |= 1ULL << kTcrIrgn0Shift;
  tcr |= 1ULL << kTcrOrgn0Shift;
  tcr |= 3ULL << kTcrSh0Shift;
  // Tg0 = 0 selects the 4 KiB granule.
  WriteTcrEl1(tcr);

  DsbSy();
  WriteTtbr0El1(ttbr0);
  DsbSy();
  InstructionSync();
  InvalidateTlbAllLocal();

  uint64_t sctlr = ReadSctlrEl1();
  sctlr |= kSctlrMmuEnable | kSctlrDataCacheEnable | kSctlrInstructionCacheEnable;
  WriteSctlrEl1(sctlr);
  InstructionSync();
  InvalidateIcacheAll();

  return Status::kOk;
}

bool IsMmuEnabled() noexcept {
  return (ReadSctlrEl1() & kSctlrMmuEnable) != 0;
}

bool PanSupported() noexcept {
  constexpr uint32_t kPanShift = 20U;
  constexpr uint64_t kPanMask = 0xFULL << kPanShift;
  return (ReadIdAa64Mmfr1El1() & kPanMask) != 0U;
}

void EnablePan() noexcept {
  constexpr uint64_t kSctlrSpan = 1ULL << 23U;
  const uint64_t sctlr = ReadSctlrEl1() & ~kSctlrSpan;
  WriteSctlrEl1(sctlr);
  InstructionSync();
}

void SwitchAddressSpace(uint64_t ttbr0, uint16_t asid) noexcept {
  const uint64_t value = ttbr0 | (static_cast<uint64_t>(asid) << kTtbrAsidShift);
  DsbSy();
  WriteTtbr0El1(value);
  DsbSy();
  InstructionSync();
}

}  // namespace og::arch::aarch64
