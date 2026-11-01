#include "og/dma/dma.h"

#include "memory.h"

namespace og::dma {

namespace {

constexpr uint32_t kMaxBlocks = 64;
constexpr uint64_t kMinBlockSize = 8;

}  // namespace

Status Pool::init(uint64_t block_size, uint32_t block_count) noexcept {
  if (ready_) {
    return Status::kOk;
  }
  if (block_size < kMinBlockSize) {
    return Status::kInvalidArg;
  }
  if (block_count == 0 || block_count > kMaxBlocks) {
    return Status::kInvalidArg;
  }

  const Result<DmaBuffer> backing = AllocCoherent(block_size * block_count, 4096);
  if (backing.is_err()) {
    return backing.status();
  }

  backing_ = backing.value();
  block_size_ = static_cast<uint32_t>(block_size);
  block_count_ = block_count;
  free_count_ = block_count;
  used_bitmap_ = 0;

  auto* base = static_cast<uint8_t*>(backing_.cpu_address);
  for (uint32_t index = 0; index < block_count; ++index) {
    auto* block = reinterpret_cast<void**>(base + (index * block_size_));
    const uint32_t next = index + 1U;
    *block = (next < block_count) ? static_cast<void*>(base + (next * block_size_)) : nullptr;
  }
  free_list_ = base;

  ready_ = true;
  return Status::kOk;
}

Result<DmaBuffer> Pool::alloc() noexcept {
  if (!ready_) {
    return Result<DmaBuffer>::Err(Status::kNotInit);
  }
  if (free_list_ == nullptr) {
    return Result<DmaBuffer>::Err(Status::kNoMem);
  }

  auto* block = static_cast<uint8_t*>(free_list_);
  const uint64_t offset = static_cast<uint64_t>(block - static_cast<uint8_t*>(backing_.cpu_address));
  const uint32_t index = static_cast<uint32_t>(offset / block_size_);
  if (index >= block_count_) {
    return Result<DmaBuffer>::Err(Status::kIoError);
  }

  free_list_ = *reinterpret_cast<void**>(block);
  used_bitmap_ |= (1ULL << index);
  --free_count_;

  DmaBuffer buffer{};
  buffer.cpu_address = block;
  buffer.device_address = backing_.device_address + offset;
  buffer.size = block_size_;
  return Result<DmaBuffer>::Ok(buffer);
}

Status Pool::free(DmaBuffer buffer) noexcept {
  if (!ready_) {
    return Status::kNotInit;
  }
  if (buffer.cpu_address == nullptr) {
    return Status::kInvalidArg;
  }

  auto* base = static_cast<uint8_t*>(backing_.cpu_address);
  auto* block = static_cast<uint8_t*>(buffer.cpu_address);
  if (block < base || block >= (base + backing_.size)) {
    return Status::kInvalidArg;
  }
  const uint64_t offset = static_cast<uint64_t>(block - base);
  if ((offset % block_size_) != 0U) {
    return Status::kUnaligned;
  }
  const uint32_t index = static_cast<uint32_t>(offset / block_size_);
  if (index >= block_count_) {
    return Status::kInvalidArg;
  }
  if ((used_bitmap_ & (1ULL << index)) == 0U) {
    return Status::kInvalidArg;
  }

  used_bitmap_ &= ~(1ULL << index);
  *reinterpret_cast<void**>(block) = free_list_;
  free_list_ = block;
  ++free_count_;
  return Status::kOk;
}

Status Pool::release() noexcept {
  if (!ready_) {
    return Status::kNotInit;
  }
  if (free_count_ != block_count_) {
    return Status::kBusy;
  }
  const Status status = FreeCoherent(backing_);
  if (IsErr(status)) {
    return status;
  }
  backing_ = DmaBuffer{};
  block_size_ = 0;
  block_count_ = 0;
  free_count_ = 0;
  free_list_ = nullptr;
  used_bitmap_ = 0;
  ready_ = false;
  return Status::kOk;
}

}  // namespace og::dma
