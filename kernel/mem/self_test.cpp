#include "og/mem_selftest.h"

#include "memory.h"
#include "og/heap.h"
#include "og/log.h"
#include "og/pmm.h"
#include "og/vmm.h"

namespace og::mem {

namespace {

constexpr uint64_t kPageRunTest = 256;
constexpr uint64_t kChurnIterations = 10000;
constexpr uint64_t kTestVirtual4K = 0x100000000ULL;   // 4 GiB
constexpr uint64_t kTestVirtual2M = 0x120000000ULL;   // 4.5 GiB
constexpr uint64_t kPatternSize = 64;

[[nodiscard]] Status TestPmmPages() noexcept {
  const uint64_t free_before = PmmFreeFrames();
  uint64_t addresses[kPageRunTest];

  for (uint64_t index = 0; index < kPageRunTest; ++index) {
    const Result<uint64_t> page = PmmAllocPage();
    if (page.is_err()) {
      for (uint64_t undo = 0; undo < index; ++undo) {
        (void)PmmFreePage(addresses[undo]);
      }
      return Status::kNoMem;
    }
    if ((page.value() % kPageSize) != 0) {
      return Status::kUnaligned;
    }
    addresses[index] = page.value();
  }

  for (uint64_t index = 0; index < kPageRunTest; ++index) {
    for (uint64_t other = index + 1; other < kPageRunTest; ++other) {
      if (addresses[index] == addresses[other]) {
        return Status::kInvalidArg;
      }
    }
  }

  for (uint64_t index = 0; index < kPageRunTest; ++index) {
    const Status status = PmmFreePage(addresses[index]);
    if (IsErr(status)) {
      return status;
    }
  }

  if (PmmFreeFrames() != free_before) {
    return Status::kInvalidArg;
  }

  for (uint64_t iteration = 0; iteration < kChurnIterations; ++iteration) {
    const Result<uint64_t> page = PmmAllocPage();
    if (page.is_err()) {
      return Status::kNoMem;
    }
    const Status status = PmmFreePage(page.value());
    if (IsErr(status)) {
      return status;
    }
  }

  if (PmmFreeFrames() != free_before) {
    return Status::kInvalidArg;
  }
  return Status::kOk;
}

[[nodiscard]] Status TestVmm4K() noexcept {
  const Result<uint64_t> page = PmmAllocPage();
  if (page.is_err()) {
    return Status::kNoMem;
  }
  const uint64_t physical = page.value();

  Status status = VmmMap(kTestVirtual4K, physical, kPageSize, arch::aarch64::MemoryKind::kNormal,
                         false);
  if (IsErr(status)) {
    (void)PmmFreePage(physical);
    return status;
  }

  auto* target = reinterpret_cast<void*>(kTestVirtual4K);
  mem::Fill(target, 0xA5, kPatternSize);
  uint8_t expected[kPatternSize];
  mem::Fill(expected, 0xA5, kPatternSize);
  if (!mem::Equal(target, expected, kPatternSize)) {
    return Status::kIoError;
  }

  const Result<uint64_t> translated = VmmTranslate(kTestVirtual4K + 8);
  if (translated.is_err()) {
    return translated.status();
  }
  if (translated.value() != (physical + 8)) {
    return Status::kInvalidArg;
  }

  status = VmmUnmap(kTestVirtual4K, kPageSize);
  if (IsErr(status)) {
    return status;
  }
  return PmmFreePage(physical);
}

[[nodiscard]] Status TestVmm2M() noexcept {
  constexpr uint64_t kPages2M = 512;
  const Result<uint64_t> run = PmmAllocPages(kPages2M);
  if (run.is_err()) {
    Log(LogLevel::kWarn, "mem test: 2m block skipped, no contiguous run\n");
    return Status::kOk;
  }
  const uint64_t physical = run.value();

  Status status = VmmMap(kTestVirtual2M, physical, arch::aarch64::kBlockSize2M,
                         arch::aarch64::MemoryKind::kNormal, false);
  if (IsErr(status)) {
    (void)PmmFreePages(physical, kPages2M);
    return status;
  }

  auto* first = reinterpret_cast<uint8_t*>(kTestVirtual2M);
  auto* last = reinterpret_cast<uint8_t*>(kTestVirtual2M + arch::aarch64::kBlockSize2M);
  mem::Fill(first, 0x11, kPatternSize);
  mem::Fill(last - 1, 0x22, 1);

  const Result<uint64_t> translated_last =
    VmmTranslate(kTestVirtual2M + arch::aarch64::kBlockSize2M - 1);
  if (translated_last.is_err()) {
    return translated_last.status();
  }
  if (translated_last.value() != (physical + arch::aarch64::kBlockSize2M - 1)) {
    return Status::kInvalidArg;
  }
  if (*(last - 1) != 0x22) {
    return Status::kIoError;
  }

  status = VmmUnmap(kTestVirtual2M, arch::aarch64::kBlockSize2M);
  if (IsErr(status)) {
    return status;
  }
  return PmmFreePages(physical, kPages2M);
}

[[nodiscard]] Status TestHeap() noexcept {
  constexpr uint64_t kSmall = 32;
  constexpr uint64_t kMedium = 128;
  constexpr uint64_t kLarge = 3000;

  Result<void*> small = HeapAlloc(kSmall);
  if (small.is_err()) {
    return small.status();
  }
  Result<void*> medium = HeapAlloc(kMedium);
  if (medium.is_err()) {
    (void)HeapFree(small.value());
    return medium.status();
  }
  Result<void*> large = HeapZalloc(kLarge);
  if (large.is_err()) {
    (void)HeapFree(small.value());
    (void)HeapFree(medium.value());
    return large.status();
  }
  if (small.value() == medium.value()) {
    return Status::kInvalidArg;
  }

  mem::Fill(small.value(), 0x5A, kSmall);
  mem::Fill(medium.value(), 'B', kMedium);
  uint8_t expected[kSmall];
  mem::Fill(expected, 0x5A, kSmall);
  if (!mem::Equal(small.value(), expected, kSmall)) {
    return Status::kIoError;
  }

  Result<void*> grown = HeapRealloc(medium.value(), 512);
  if (grown.is_err()) {
    return grown.status();
  }
  const auto* grown_bytes = static_cast<const uint8_t*>(grown.value());
  if (grown_bytes[0] != 'B' || grown_bytes[kMedium - 1] != 'B') {
    return Status::kInvalidArg;
  }

  Status status = HeapFree(small.value());
  if (IsErr(status)) {
    return status;
  }
  status = HeapFree(grown.value());
  if (IsErr(status)) {
    return status;
  }
  status = HeapFree(large.value());
  if (IsErr(status)) {
    return status;
  }

  const HeapStats stats = GetHeapStats();
  if (stats.allocations != stats.frees) {
    return Status::kInvalidArg;
  }
  return Status::kOk;
}

}  // namespace

Status RunSelfTest() noexcept {
  Status status = TestPmmPages();
  if (IsErr(status)) {
    Log(LogLevel::kError, "mem test: pmm failed\n");
    return status;
  }
  Log(LogLevel::kInfo, "mem test: pmm ok\n");

  status = TestVmm4K();
  if (IsErr(status)) {
    Log(LogLevel::kError, "mem test: vmm 4k failed\n");
    return status;
  }
  Log(LogLevel::kInfo, "mem test: vmm 4k ok\n");

  status = TestVmm2M();
  if (IsErr(status)) {
    Log(LogLevel::kError, "mem test: vmm 2m failed\n");
    return status;
  }
  Log(LogLevel::kInfo, "mem test: vmm 2m ok\n");

  status = TestHeap();
  if (IsErr(status)) {
    Log(LogLevel::kError, "mem test: heap failed\n");
    return status;
  }
  Log(LogLevel::kInfo, "mem test: heap ok\n");
  return Status::kOk;
}

}  // namespace og::mem
