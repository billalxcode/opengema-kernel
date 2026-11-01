#include "og/dma_selftest.h"

#include "memory.h"
#include "og/dma.h"
#include "og/heap.h"
#include "og/log.h"
#include "og/pmm.h"

namespace og::dma {

namespace {

constexpr uint64_t kBufferSize = 8192;
constexpr uint64_t kPatternSize = 128;

[[nodiscard]] Status TestCoherent() noexcept {
  const uint64_t free_before = mem::PmmFreeFrames();

  Result<DmaBuffer> buffer = AllocCoherent(kBufferSize, 4096);
  if (buffer.is_err()) {
    return buffer.status();
  }
  const DmaBuffer region = buffer.value();
  if (region.cpu_address == nullptr) {
    return Status::kInvalidArg;
  }
  if (region.size < kBufferSize) {
    return Status::kInvalidArg;
  }
  if (reinterpret_cast<uint64_t>(region.cpu_address) != region.device_address) {
    return Status::kInvalidArg;
  }
  if ((region.device_address % 4096U) != 0) {
    return Status::kUnaligned;
  }

  mem::Fill(region.cpu_address, 0x3C, kPatternSize);
  Status status = SyncForDevice(region.cpu_address, kPatternSize);
  if (IsErr(status)) {
    return status;
  }
  uint8_t expected[kPatternSize];
  mem::Fill(expected, 0x3C, kPatternSize);
  if (!mem::Equal(region.cpu_address, expected, kPatternSize)) {
    return Status::kIoError;
  }

  status = FreeCoherent(region);
  if (IsErr(status)) {
    return status;
  }
  if (mem::PmmFreeFrames() != free_before) {
    return Status::kInvalidArg;
  }
  return Status::kOk;
}

[[nodiscard]] Status TestStreaming() noexcept {
  Result<void*> block = mem::HeapAlloc(256);
  if (block.is_err()) {
    return block.status();
  }

  Result<DmaAddress> mapped = MapStreaming(block.value(), 256, DmaDirection::kToDevice);
  if (mapped.is_err()) {
    (void)mem::HeapFree(block.value());
    return mapped.status();
  }
  if (mapped.value() != reinterpret_cast<uint64_t>(block.value())) {
    return Status::kInvalidArg;
  }

  Status status = UnmapStreaming(block.value(), 256, DmaDirection::kToDevice);
  if (IsErr(status)) {
    return status;
  }
  Result<DmaAddress> from_device = MapStreaming(block.value(), 256, DmaDirection::kFromDevice);
  if (from_device.is_err()) {
    return from_device.status();
  }
  status = UnmapStreaming(block.value(), 256, DmaDirection::kFromDevice);
  if (IsErr(status)) {
    return status;
  }

  return mem::HeapFree(block.value());
}

}  // namespace

Status RunDmaSelfTest() noexcept {
  Status status = TestCoherent();
  if (IsErr(status)) {
    Log(LogLevel::kError, "dma test: coherent failed\n");
    return status;
  }
  Log(LogLevel::kInfo, "dma test: coherent ok\n");

  status = TestStreaming();
  if (IsErr(status)) {
    Log(LogLevel::kError, "dma test: streaming failed\n");
    return status;
  }
  Log(LogLevel::kInfo, "dma test: streaming ok\n");
  return Status::kOk;
}

}  // namespace og::dma
