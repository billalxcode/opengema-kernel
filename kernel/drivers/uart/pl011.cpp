#include "og/drivers/uart.h"

#include "og/events/irq.h"
#include "og/io/register.h"

namespace og::drivers::uart {

namespace {

constexpr uint64_t kDataRegister = 0x00;
constexpr uint64_t kFlagRegister = 0x18;
constexpr uint64_t kPeripheralId0 = 0xFE0;
constexpr uint64_t kPeripheralId1 = 0xFE4;
constexpr uint64_t kMinimumSpan = 0x1000;
constexpr uint64_t kIntegerBaudRegister = 0x24;
constexpr uint64_t kFractionalBaudRegister = 0x28;
constexpr uint64_t kLineControlRegister = 0x2C;
constexpr uint64_t kControlRegister = 0x30;
constexpr uint64_t kInterruptMaskRegister = 0x38;
constexpr uint64_t kMaskedInterruptStatus = 0x40;
constexpr uint64_t kInterruptClearRegister = 0x44;

constexpr uint32_t kExpectedId0 = 0x11;
constexpr uint32_t kExpectedId1 = 0x10;

// FR bit 5: TX FIFO full. FR bit 4: RX FIFO empty.
using TxFifoFull = io::Field<uint32_t, 5, 1>;

constexpr uint32_t kFlagRxFifoEmpty = 1U << 4;
constexpr uint32_t kLineControlFifo = 1U << 4;
constexpr uint32_t kLineControlEightBits = 3U << 5;
constexpr uint32_t kControlEnable = 1U << 0;
constexpr uint32_t kControlTxEnable = 1U << 8;
constexpr uint32_t kControlRxEnable = 1U << 9;
constexpr uint32_t kInterruptReceive = 1U << 4;
constexpr uint32_t kInterruptClearAll = 0x7FFU;

constexpr uint32_t kTxSpinLimit = 100000U;

Pl011* g_active = nullptr;

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

Status Pl011::enable_receive(uint32_t irq, uint64_t clock_hz, uint32_t baud) noexcept {
  if (!ready_) {
    return Status::kNotInit;
  }
  if (clock_hz == 0 || baud == 0) {
    return Status::kInvalidArg;
  }

  const uint64_t denominator = static_cast<uint64_t>(baud) * 16U;
  const uint32_t integer_divisor = static_cast<uint32_t>(clock_hz / denominator);
  const uint64_t remainder = clock_hz % denominator;
  const uint32_t fractional =
    static_cast<uint32_t>(((remainder * 64U) + (denominator / 2U)) / denominator);
  if (integer_divisor == 0) {
    return Status::kInvalidArg;
  }

  Status status = regs_.Write32(kIntegerBaudRegister, integer_divisor);
  if (IsErr(status)) {
    return status;
  }
  status = regs_.Write32(kFractionalBaudRegister, fractional);
  if (IsErr(status)) {
    return status;
  }
  status = regs_.Write32(kLineControlRegister, kLineControlFifo | kLineControlEightBits);
  if (IsErr(status)) {
    return status;
  }
  status = regs_.Write32(kControlRegister, kControlEnable | kControlTxEnable | kControlRxEnable);
  if (IsErr(status)) {
    return status;
  }
  (void)regs_.Write32(kInterruptClearRegister, kInterruptClearAll);
  status = regs_.Write32(kInterruptMaskRegister, kInterruptReceive);
  if (IsErr(status)) {
    return status;
  }

  status = events::RegisterIrq(irq, &RxHandler, this);
  if (IsErr(status)) {
    return status;
  }
  rx_enabled_ = true;
  return Status::kOk;
}

Status Pl011::RxHandler(void* context, uint32_t /*irq*/) noexcept {
  auto* self = static_cast<Pl011*>(context);
  if (self == nullptr) {
    return Status::kInvalidArg;
  }

  const Result<uint32_t> masked = self->regs_.Read32(kMaskedInterruptStatus);
  if (masked.is_err()) {
    return masked.status();
  }
  if ((masked.value() & kInterruptReceive) != 0U) {
    for (uint32_t spin = 0; spin < kTxSpinLimit; ++spin) {
      const Result<uint32_t> flags = self->regs_.Read32(kFlagRegister);
      if (flags.is_err()) {
        return flags.status();
      }
      if ((flags.value() & kFlagRxFifoEmpty) != 0U) {
        break;
      }
      const Result<uint32_t> data = self->regs_.Read32(kDataRegister);
      if (data.is_err()) {
        return data.status();
      }
      self->push(static_cast<char>(data.value() & 0xFFU));
    }
  }

  (void)self->regs_.Write32(kInterruptClearRegister, kInterruptClearAll);
  return Status::kOk;
}

void Pl011::push(char value) noexcept {
  const uint32_t head = __atomic_load_n(&ring_head_, __ATOMIC_RELAXED);
  const uint32_t tail = __atomic_load_n(&ring_tail_, __ATOMIC_ACQUIRE);
  if ((head - tail) >= kRingCapacity) {
    return;
  }
  ring_[head % kRingCapacity] = value;
  __atomic_store_n(&ring_head_, head + 1U, __ATOMIC_RELEASE);
}

uint32_t Pl011::read(std::span<char> out) noexcept {
  if (!rx_enabled_ || out.empty()) {
    return 0;
  }
  uint32_t copied = 0;
  for (;;) {
    const uint32_t tail = __atomic_load_n(&ring_tail_, __ATOMIC_RELAXED);
    const uint32_t head = __atomic_load_n(&ring_head_, __ATOMIC_ACQUIRE);
    if (tail == head) {
      break;
    }
    out[copied] = ring_[tail % kRingCapacity];
    __atomic_store_n(&ring_tail_, tail + 1U, __ATOMIC_RELEASE);
    ++copied;
    if (copied == out.size()) {
      break;
    }
  }
  return copied;
}

uint32_t Pl011::available() const noexcept {
  const uint32_t head = __atomic_load_n(&ring_head_, __ATOMIC_ACQUIRE);
  const uint32_t tail = __atomic_load_n(&ring_tail_, __ATOMIC_ACQUIRE);
  return head - tail;
}

void SetActive(Pl011* uart) noexcept {
  g_active = uart;
}

Pl011* Active() noexcept {
  return g_active;
}

}  // namespace og::drivers::uart
