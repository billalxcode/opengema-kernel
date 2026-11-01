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

[[nodiscard]] Status TestPool() noexcept {
  Pool pool;
  Status status = pool.init(256, 4);
  if (IsErr(status)) {
    return status;
  }
  if (pool.available() != 4U) {
    return Status::kInvalidArg;
  }

  DmaBuffer buffers[4];
  for (uint32_t index = 0; index < 4U; ++index) {
    const Result<DmaBuffer> block = pool.alloc();
    if (block.is_err()) {
      return block.status();
    }
    if (block.value().size != 256U) {
      return Status::kInvalidArg;
    }
    buffers[index] = block.value();
    for (uint32_t other = 0; other < index; ++other) {
      if (buffers[other].cpu_address == buffers[index].cpu_address) {
        return Status::kInvalidArg;
      }
    }
  }
  if (pool.available() != 0U) {
    return Status::kInvalidArg;
  }
  if (pool.alloc().is_ok()) {
    return Status::kInvalidArg;
  }

  status = pool.free(buffers[0]);
  if (IsErr(status)) {
    return status;
  }
  if (IsOk(pool.free(buffers[0]))) {
    return Status::kInvalidArg;  // double free must fail
  }
  status = pool.free(buffers[1]);
  if (IsErr(status)) {
    return status;
  }

  const Result<DmaBuffer> first = pool.alloc();
  const Result<DmaBuffer> second = pool.alloc();
  if (first.is_err() || second.is_err()) {
    return Status::kNoMem;
  }

  status = pool.free(buffers[2]);
  if (IsErr(status)) {
    return status;
  }
  status = pool.free(buffers[3]);
  if (IsErr(status)) {
    return status;
  }
  status = pool.free(first.value());
  if (IsErr(status)) {
    return status;
  }
  status = pool.free(second.value());
  if (IsErr(status)) {
    return status;
  }
  if (pool.available() != 4U) {
    return Status::kInvalidArg;
  }
  return pool.release();
}

}  // namespace

Status RunDmaSelfTest() noexcept {  Status status = TestCoherent();
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

  status = TestPool();
  if (IsErr(status)) {
    Log(LogLevel::kError, "dma test: pool failed\n");
    return status;
  }
  Log(LogLevel::kInfo, "dma test: pool ok\n");
  return Status::kOk;
}

}  // namespace og::dma
