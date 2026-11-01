#include "og/uart.h"

#include "og/register.h"

namespace og::drivers::uart {

namespace {

constexpr uint64_t kDataRegister = 0x00;
constexpr uint64_t kFlagRegister = 0x18;
constexpr uint64_t kPeripheralId0 = 0xFE0;
constexpr uint64_t kPeripheralId1 = 0xFE4;
constexpr uint64_t kMinimumSpan = 0x1000;

constexpr uint32_t kExpectedId0 = 0x11;
constexpr uint32_t kExpectedId1 = 0x10;

// FR bit 5: TX FIFO full.
using TxFifoFull = io::Field<uint32_t, 5, 1>;

constexpr uint32_t kTxSpinLimit = 100000U;

Status WriteTrampoline(void* context, std::span<const uint8_t> bytes) noexcept {
  if (context == nullptr) {
    return Status::kInvalidArg;
  }
  return static_cast<Pl011*>(context)->write(bytes);
}

}  // namespace

Status Pl011::init(uint64_t base, uint64_t size) noexcept {
  if (base == 0) {
    return Status::kInvalidArg;
  }
  if (size < kMinimumSpan) {
    return Status::kInvalidArg;
  }

  regs_ = io::MmioRegion(base, size);

  // PrimeCell IDs prove the device is really there, not just mapped.
  const Result<uint32_t> id0 = regs_.Read32(kPeripheralId0);
  if (id0.is_err()) {
    return id0.status();
  }
  if (id0.value() != kExpectedId0) {
    return Status::kNoDevice;
  }
  const Result<uint32_t> id1 = regs_.Read32(kPeripheralId1);
  if (id1.is_err()) {
    return id1.status();
  }
  if (id1.value() != kExpectedId1) {
    return Status::kNoDevice;
  }

  ready_ = true;
  return Status::kOk;
}

Status Pl011::write(std::span<const uint8_t> bytes) noexcept {
  if (!ready_) {
    return Status::kNotInit;
  }
  if (bytes.empty()) {
    return Status::kInvalidArg;
  }
  for (uint8_t byte : bytes) {
    const Status status = wait_tx_ready();
    if (IsErr(status)) {
      return status;
    }
    const Status write_status = regs_.Write32(kDataRegister, static_cast<uint32_t>(byte));
    if (IsErr(write_status)) {
      return write_status;
    }
  }
  return Status::kOk;
}

Status Pl011::wait_tx_ready() noexcept {
  for (uint32_t spin = 0; spin < kTxSpinLimit; ++spin) {
    const Result<uint32_t> flags = regs_.Read32(kFlagRegister);
    if (flags.is_err()) {
      return flags.status();
    }
    if (TxFifoFull::Decode(flags.value()) == 0U) {
      return Status::kOk;
    }
  }
  return Status::kTimeout;
}

io::Console MakeConsole(Pl011& uart) noexcept {
  if (!uart.is_ready()) {
    return io::Console{};
  }
  return io::Console(&WriteTrampoline, &uart);
}

}  // namespace og::drivers::uart
