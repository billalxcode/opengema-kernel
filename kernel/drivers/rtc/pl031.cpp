#include "og/drivers/rtc.h"

namespace og::drivers::rtc {

namespace {

constexpr uint64_t kDataRegister = 0x00;
constexpr uint64_t kControlRegister = 0x0C;
constexpr uint64_t kMinimumSpan = 0x1000;
constexpr uint32_t kControlEnable = 1U;

}  // namespace

Status Pl031::init(uint64_t base, uint64_t size) noexcept {
  if (base == 0) {
    return Status::kInvalidArg;
  }
  if (size < kMinimumSpan) {
    return Status::kInvalidArg;
  }

  registers_ = io::MmioRegion(base, size);
  const Status status = registers_.Write32(kControlRegister, kControlEnable);
  if (IsErr(status)) {
    return status;
  }
  ready_ = true;
  return Status::kOk;
}

Result<uint64_t> Pl031::read_unix_seconds() noexcept {
  if (!ready_) {
    return Result<uint64_t>::Err(Status::kNotInit);
  }
  const Result<uint32_t> value = registers_.Read32(kDataRegister);
  if (value.is_err()) {
    return Result<uint64_t>::Err(value.status());
  }
  return Result<uint64_t>::Ok(value.value());
}

}  // namespace og::drivers::rtc
