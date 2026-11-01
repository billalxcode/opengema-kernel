#include "og/dma/dma.h"

#include "memory.h"
#include "og/arch/cache.h"
#include "og/mem/pmm.h"

namespace og::dma {

namespace {

[[nodiscard]] bool IsPowerOfTwo(uint64_t value) noexcept {
  if (value == 0) {
    return false;
  }
  return (value & (value - 1U)) == 0;
}

[[nodiscard]] uint64_t AlignUp(uint64_t value, uint64_t alignment) noexcept {
  return (value + alignment - 1U) & ~(alignment - 1U);
}

}  // namespace

Result<DmaBuffer> AllocCoherent(uint64_t size, uint64_t alignment) noexcept {
  if (size == 0) {
    return Result<DmaBuffer>::Err(Status::kInvalidArg);
  }
  const uint64_t effective_alignment = alignment == 0 ? 1U : alignment;
  if (!IsPowerOfTwo(effective_alignment)) {
    return Result<DmaBuffer>::Err(Status::kInvalidArg);
  }
  if (effective_alignment > mem::kPageSize) {
    return Result<DmaBuffer>::Err(Status::kUnsupported);
  }

  const uint64_t total = AlignUp(size, mem::kPageSize);
  const uint64_t pages = total / mem::kPageSize;
  const Result<uint64_t> address = mem::PmmAllocPages(pages);
  if (address.is_err()) {
    return Result<DmaBuffer>::Err(address.status());
  }

  auto* cpu = reinterpret_cast<void*>(address.value());
  mem::Zero(cpu, total);

  DmaBuffer buffer{};
  buffer.cpu_address = cpu;
  buffer.device_address = address.value();
  buffer.size = total;
  return Result<DmaBuffer>::Ok(buffer);
}

Status FreeCoherent(DmaBuffer buffer) noexcept {
  if (buffer.cpu_address == nullptr) {
    return Status::kInvalidArg;
  }
  if (buffer.size == 0) {
    return Status::kInvalidArg;
  }
  if ((buffer.size % mem::kPageSize) != 0) {
    return Status::kInvalidArg;
  }
  return mem::PmmFreePages(buffer.device_address, buffer.size / mem::kPageSize);
}

Result<DmaAddress> MapStreaming(void* cpu_address, uint64_t size,
                                DmaDirection direction) noexcept {
  if (cpu_address == nullptr) {
    return Result<DmaAddress>::Err(Status::kInvalidArg);
  }
  if (size == 0) {
    return Result<DmaAddress>::Err(Status::kInvalidArg);
  }

  if (direction == DmaDirection::kToDevice || direction == DmaDirection::kBoth) {
    const Status status = SyncForDevice(cpu_address, size);
    if (IsErr(status)) {
      return Result<DmaAddress>::Err(status);
    }
  } else {
    const Status status = SyncForCpu(cpu_address, size);
    if (IsErr(status)) {
      return Result<DmaAddress>::Err(status);
    }
  }

  return Result<DmaAddress>::Ok(reinterpret_cast<uint64_t>(cpu_address));
}

Status UnmapStreaming(void* cpu_address, uint64_t size, DmaDirection direction) noexcept {
  if (cpu_address == nullptr) {
    return Status::kInvalidArg;
  }
  if (size == 0) {
    return Status::kInvalidArg;
  }
  if (direction == DmaDirection::kFromDevice || direction == DmaDirection::kBoth) {
    return SyncForCpu(cpu_address, size);
  }
  return Status::kOk;
}

Status SyncForDevice(void* cpu_address, uint64_t size) noexcept {
  if (cpu_address == nullptr) {
    return Status::kInvalidArg;
  }
  if (size == 0) {
    return Status::kInvalidArg;
  }
  const uint64_t begin = reinterpret_cast<uint64_t>(cpu_address);
  arch::aarch64::CleanInvalidateDcacheRange(begin, begin + size);
  return Status::kOk;
}

Status SyncForCpu(void* cpu_address, uint64_t size) noexcept {
  if (cpu_address == nullptr) {
    return Status::kInvalidArg;
  }
  if (size == 0) {
    return Status::kInvalidArg;
  }
  const uint64_t begin = reinterpret_cast<uint64_t>(cpu_address);
  arch::aarch64::InvalidateDcacheRange(begin, begin + size);
  return Status::kOk;
}

}  // namespace og::dma
