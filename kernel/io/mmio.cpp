#include "og/io/mmio.h"

#include "og/arch/barriers.h"

namespace og::io {

namespace {

// Shared implementation for every access width. Keeps bounds, alignment,
// ordering, and error mapping in one place instead of eight copies.
template <typename T>
[[nodiscard]] Result<T> ReadAt(const MmioRegion& region, uint64_t offset) noexcept {
  if (!region.valid()) {
    return Result<T>::Err(Status::kNotInit);
  }
  if (!region.Contains(offset, sizeof(T))) {
    return Result<T>::Err(Status::kInvalidArg);
  }
  if ((offset % sizeof(T)) != 0) {
    return Result<T>::Err(Status::kUnaligned);
  }
  const auto* reg = reinterpret_cast<volatile const T*>(region.base() + offset);
  const T value = *reg;
  arch::aarch64::DmbSy();
  return Result<T>::Ok(value);
}

template <typename T>
[[nodiscard]] Status WriteAt(const MmioRegion& region, uint64_t offset, T value) noexcept {
  if (!region.valid()) {
    return Status::kNotInit;
  }
  if (!region.Contains(offset, sizeof(T))) {
    return Status::kInvalidArg;
  }
  if ((offset % sizeof(T)) != 0) {
    return Status::kUnaligned;
  }
  auto* reg = reinterpret_cast<volatile T*>(region.base() + offset);
  *reg = value;
  arch::aarch64::DmbSy();
  return Status::kOk;
}

}  // namespace

Result<uint8_t> MmioRegion::Read8(uint64_t offset) const noexcept {
  return ReadAt<uint8_t>(*this, offset);
}

Result<uint16_t> MmioRegion::Read16(uint64_t offset) const noexcept {
  return ReadAt<uint16_t>(*this, offset);
}

Result<uint32_t> MmioRegion::Read32(uint64_t offset) const noexcept {
  return ReadAt<uint32_t>(*this, offset);
}

Result<uint64_t> MmioRegion::Read64(uint64_t offset) const noexcept {
  return ReadAt<uint64_t>(*this, offset);
}

Status MmioRegion::Write8(uint64_t offset, uint8_t value) const noexcept {
  return WriteAt<uint8_t>(*this, offset, value);
}

Status MmioRegion::Write16(uint64_t offset, uint16_t value) const noexcept {
  return WriteAt<uint16_t>(*this, offset, value);
}

Status MmioRegion::Write32(uint64_t offset, uint32_t value) const noexcept {
  return WriteAt<uint32_t>(*this, offset, value);
}

Status MmioRegion::Write64(uint64_t offset, uint64_t value) const noexcept {
  return WriteAt<uint64_t>(*this, offset, value);
}

void MmioRegion::Sync() const noexcept {
  arch::aarch64::DsbSy();
}

}  // namespace og::io
