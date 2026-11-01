#include "og/drivers/virtio_input.h"

#include "memory.h"
#include "og/dma/dma.h"

namespace og::drivers::virtio {

namespace {

constexpr uint16_t kVirtioDeviceIdLegacyInput = 0x1012;
constexpr uint16_t kVirtioDeviceIdModernInput = 0x1052;
constexpr uint32_t kEventBufferSize = 512;
constexpr uint32_t kEventSize = 8;
constexpr uint32_t kEventCount = 64;

constexpr uint16_t kEventKey = 1;
constexpr uint16_t kEventRel = 2;
constexpr uint16_t kRelX = 0;
constexpr uint16_t kRelY = 1;
constexpr uint16_t kRelWheel = 8;
constexpr uint16_t kButtonFirst = 0x110;
constexpr uint16_t kButtonLast = 0x117;

struct VirtioInputEvent {
  uint16_t type;
  uint16_t code;
  uint32_t value;
};

static_assert(sizeof(VirtioInputEvent) == kEventSize);

[[nodiscard]] uint16_t HidUsage(uint16_t code) noexcept {
  switch (code) {
    case 1:
      return 0x29;  // Escape
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
      return static_cast<uint16_t>(0x1E + (code - 2U));  // 1..0
    case 12:
      return 0x2D;
    case 13:
      return 0x2E;
    case 14:
      return 0x2A;
    case 15:
      return 0x2B;
    case 16:
      return 0x14;  // Q
    case 17:
      return 0x1A;  // W
    case 18:
      return 0x08;  // E
    case 19:
      return 0x15;  // R
    case 20:
      return 0x17;  // T
    case 21:
      return 0x1C;  // Y
    case 22:
      return 0x18;  // U
    case 23:
      return 0x0C;  // I
    case 24:
      return 0x12;  // O
    case 25:
      return 0x13;  // P
    case 26:
      return 0x2F;
    case 27:
      return 0x30;
    case 28:
      return 0x28;
    case 29:
      return 0xE0;
    case 30:
      return 0x04;  // A
    case 31:
      return 0x16;  // S
    case 32:
      return 0x07;  // D
    case 33:
      return 0x09;  // F
    case 34:
      return 0x0A;  // G
    case 35:
      return 0x0B;  // H
    case 36:
      return 0x0D;  // J
    case 37:
      return 0x0E;  // K
    case 38:
      return 0x0F;  // L
    case 39:
      return 0x33;
    case 40:
      return 0x34;
    case 41:
      return 0x35;
    case 42:
      return 0xE1;
    case 43:
      return 0x31;
    case 44:
      return 0x1D;  // Z
    case 45:
      return 0x1B;  // X
    case 46:
      return 0x06;  // C
    case 47:
      return 0x19;  // V
    case 48:
      return 0x05;  // B
    case 49:
      return 0x11;  // N
    case 50:
      return 0x10;  // M
    case 51:
      return 0x36;
    case 52:
      return 0x37;
    case 53:
      return 0x38;
    case 54:
      return 0xE5;
    case 56:
      return 0xE2;
    case 57:
      return 0x2C;
    case 58:
      return 0x39;
    case 97:
      return 0xE4;
    case 100:
      return 0xE6;
    case 102:
      return 0x4A;
    case 103:
      return 0x52;
    case 104:
      return 0x4B;
    case 105:
      return 0x50;
    case 106:
      return 0x4F;
    case 107:
      return 0x4D;
    case 108:
      return 0x51;
    case 109:
      return 0x4E;
    case 110:
      return 0x49;
    case 111:
      return 0x4C;
    case 125:
      return 0xE3;
    case 126:
      return 0xE7;
    default:
      return 0;
  }
}

}  // namespace

bool IsVirtioInput(const pci::Info& info) noexcept {
  if (info.vendor_id != pci::kVendorRedHat) {
    return false;
  }
  return info.device_id == kVirtioDeviceIdLegacyInput ||
         info.device_id == kVirtioDeviceIdModernInput;
}

Status Input::init(const pci::Info& info, hid::EventFn sink,
                   void* context) noexcept {
  if (ready_) {
    return Status::kOk;
  }
  if (sink == nullptr) {
    return Status::kInvalidArg;
  }
  sink_ = sink;
  context_ = context;

  Status status = transport_.init(info);
  if (IsErr(status)) {
    return status;
  }
  status = transport_.reset();
  if (IsErr(status)) {
    return status;
  }
  status = transport_.begin_init();
  if (IsErr(status)) {
    return status;
  }
  status = transport_.negotiate(kFeatureVersion1);
  if (IsErr(status)) {
    return status;
  }
  status = queue_.init(transport_, 0, 64);
  if (IsErr(status)) {
    return status;
  }
  status = transport_.set_driver_ok();
  if (IsErr(status)) {
    return status;
  }

  const Result<dma::DmaBuffer> events =
    dma::AllocCoherent(kEventBufferSize, 4096);
  if (events.is_err()) {
    return events.status();
  }
  events_ = events.value();
  ready_ = true;
  return Status::kOk;
}

Status Input::submit_events() noexcept {
  QueueBuffer buffers[kEventCount];
  auto* cpu = static_cast<uint8_t*>(events_.cpu_address);
  for (uint32_t index = 0; index < kEventCount; ++index) {
    buffers[index] = QueueBuffer{
      cpu + (index * kEventSize),
      events_.device_address + (index * kEventSize),
      kEventSize,
      true,
    };
  }
  return queue_.submit_each(buffers, kEventCount);
}

Status Input::poll() noexcept {
  if (!ready_) {
    return Status::kNotInit;
  }

  if (!submitted_) {
    const Status status = submit_events();
    if (IsErr(status)) {
      return status;
    }
    submitted_ = true;
    return Status::kOk;
  }

  bool ready = false;
  Status status = queue_.poll(ready);
  if (IsErr(status) || !ready) {
    return status;
  }

  submitted_ = false;
  const auto* events =
    static_cast<const VirtioInputEvent*>(events_.cpu_address);
  const uint32_t count = kEventCount;
  for (uint32_t index = 0; index < count; ++index) {
    const VirtioInputEvent& event = events[index];
    if (event.type == kEventKey) {
      if (event.code >= kButtonFirst && event.code <= kButtonLast) {
        sink_(context_, hid::Event{hid::EventType::kButton,
                                   static_cast<uint16_t>(event.code -
                                                         kButtonFirst + 1U),
                                   event.value != 0U ? 1 : 0});
        continue;
      }
      const uint16_t usage = HidUsage(event.code);
      if (usage != 0U && event.value != 2U) {
        sink_(context_, hid::Event{hid::EventType::kKey, usage,
                                   event.value != 0U ? 1 : 0});
      }
      continue;
    }
    if (event.type == kEventRel) {
      uint16_t axis = 0;
      bool has_axis = false;
      if (event.code == kRelX) {
        axis = hid::kAxisX;
        has_axis = true;
      } else if (event.code == kRelY) {
        axis = hid::kAxisY;
        has_axis = true;
      } else if (event.code == kRelWheel) {
        axis = hid::kAxisWheel;
        has_axis = true;
      }
      if (has_axis) {
        sink_(context_,
              hid::Event{hid::EventType::kAxis, axis,
                         static_cast<int32_t>(event.value)});
      }
    }
  }
  mem::Zero(events_.cpu_address, kEventBufferSize);

  if (queue_.has_pending()) {
    submitted_ = true;
    return Status::kOk;
  }

  status = submit_events();
  if (IsErr(status)) {
    submitted_ = false;
    return status;
  }
  submitted_ = true;
  return Status::kOk;
}

}  // namespace og::drivers::virtio
