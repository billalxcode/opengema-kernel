#include "og/virtio.h"

#include "og/vmm.h"

namespace og::drivers::virtio {

namespace {

// virtio_pci_common_cfg offsets.
constexpr uint64_t kDeviceFeatureSelect = 0x00;
constexpr uint64_t kDeviceFeature = 0x04;
constexpr uint64_t kDriverFeatureSelect = 0x08;
constexpr uint64_t kDriverFeature = 0x0C;
constexpr uint64_t kDeviceStatus = 0x14;
constexpr uint64_t kQueueSelect = 0x16;
constexpr uint64_t kQueueSize = 0x18;
constexpr uint64_t kQueueEnable = 0x1C;
constexpr uint64_t kQueueNotifyOffset = 0x1E;
constexpr uint64_t kQueueDescriptor = 0x20;
constexpr uint64_t kQueueAvailable = 0x28;
constexpr uint64_t kQueueUsed = 0x30;

constexpr uint8_t kStatusAcknowledge = 1U << 0U;
constexpr uint8_t kStatusDriver = 1U << 1U;
constexpr uint8_t kStatusDriverOk = 1U << 2U;
constexpr uint8_t kStatusFeaturesOk = 1U << 3U;

constexpr uint8_t kPciCapabilityVendor = 0x09;
constexpr uint8_t kCapCommon = 1;
constexpr uint8_t kCapNotify = 2;
constexpr uint8_t kCapDevice = 4;

constexpr uint16_t kPciStatusRegister = 0x06;
constexpr uint16_t kPciCapabilityPointer = 0x34;
constexpr uint16_t kPciStatusCapabilityList = 1U << 4U;

struct Capability {
  uint8_t bar;
  uint32_t offset;
  uint32_t length;
  uint32_t notify_multiplier;
};

[[nodiscard]] Result<Capability> FindCapability(pci::Address address, uint8_t cfg_type) noexcept {
  const Result<uint16_t> status = pci::Read16(address, kPciStatusRegister);
  if (status.is_err()) {
    return Result<Capability>::Err(status.status());
  }
  if ((status.value() & kPciStatusCapabilityList) == 0U) {
    return Result<Capability>::Err(Status::kUnsupported);
  }

  const Result<uint8_t> start = pci::Read8(address, kPciCapabilityPointer);
  if (start.is_err()) {
    return Result<Capability>::Err(start.status());
  }

  uint8_t pointer = static_cast<uint8_t>(start.value() & 0xFCU);
  for (uint32_t step = 0; step < 48U; ++step) {
    if (pointer < 0x40U) {
      break;
    }
    const Result<uint8_t> id = pci::Read8(address, pointer);
    const Result<uint8_t> next = pci::Read8(address, static_cast<uint16_t>(pointer + 1U));
    if (id.is_err() || next.is_err()) {
      return Result<Capability>::Err(Status::kIoError);
    }
    if (id.value() == kPciCapabilityVendor) {
      const Result<uint8_t> type = pci::Read8(address, static_cast<uint16_t>(pointer + 3U));
      if (type.is_err()) {
        return Result<Capability>::Err(type.status());
      }
      if (type.value() == cfg_type) {
        const Result<uint8_t> bar = pci::Read8(address, static_cast<uint16_t>(pointer + 4U));
        const Result<uint32_t> offset =
          pci::Read32(address, static_cast<uint16_t>(pointer + 8U));
        const Result<uint32_t> length =
          pci::Read32(address, static_cast<uint16_t>(pointer + 12U));
        if (bar.is_err() || offset.is_err() || length.is_err()) {
          return Result<Capability>::Err(Status::kIoError);
        }

        Capability capability{};
        capability.bar = bar.value();
        capability.offset = offset.value();
        capability.length = length.value();
        if (cfg_type == kCapNotify) {
          const Result<uint32_t> multiplier =
            pci::Read32(address, static_cast<uint16_t>(pointer + 16U));
          if (multiplier.is_err()) {
            return Result<Capability>::Err(multiplier.status());
          }
          capability.notify_multiplier = multiplier.value();
        }
        return Result<Capability>::Ok(capability);
      }
    }
    pointer = static_cast<uint8_t>(next.value() & 0xFCU);
  }

  return Result<Capability>::Err(Status::kNotFound);
}

[[nodiscard]] Result<io::MmioRegion> RegionFor(const pci::Info& info,
                                               const Capability& capability) noexcept {
  const Result<uint64_t> bar = pci::BarBase(info, capability.bar);
  if (bar.is_err()) {
    return Result<io::MmioRegion>::Err(bar.status());
  }
  if (capability.length == 0) {
    return Result<io::MmioRegion>::Err(Status::kInvalidArg);
  }
  return Result<io::MmioRegion>::Ok(
    io::MmioRegion(bar.value() + capability.offset, capability.length));
}

// A BAR may sit above RAM, outside the identity map built at boot. It is
// mapped as device memory page by page, skipping pages already mapped.

}  // namespace

Status PciTransport::init(const pci::Info& info) noexcept {
  if (!pci::PciReady()) {
    return Status::kNotInit;
  }

  const Result<Capability> common = FindCapability(info.address, kCapCommon);
  if (common.is_err()) {
    return common.status();
  }
  const Result<Capability> notify = FindCapability(info.address, kCapNotify);
  if (notify.is_err()) {
    return notify.status();
  }
  const Result<Capability> device = FindCapability(info.address, kCapDevice);
  if (device.is_err()) {
    return device.status();
  }

  const Result<io::MmioRegion> common_region = RegionFor(info, common.value());
  if (common_region.is_err()) {
    return common_region.status();
  }
  const Result<io::MmioRegion> notify_region = RegionFor(info, notify.value());
  if (notify_region.is_err()) {
    return notify_region.status();
  }
  const Result<io::MmioRegion> device_region = RegionFor(info, device.value());
  if (device_region.is_err()) {
    return device_region.status();
  }

  common_ = common_region.value();
  notify_ = notify_region.value();
  device_ = device_region.value();

  Status map_status = mem::VmmMapDevice(common_.base(), common_.size());
  if (IsErr(map_status)) {
    return map_status;
  }
  map_status = mem::VmmMapDevice(notify_.base(), notify_.size());
  if (IsErr(map_status)) {
    return map_status;
  }
  map_status = mem::VmmMapDevice(device_.base(), device_.size());
  if (IsErr(map_status)) {
    return map_status;
  }

  notify_multiplier_ = notify.value().notify_multiplier;
  ready_ = true;
  return Status::kOk;
}

Status PciTransport::write_status(uint8_t value) noexcept {
  if (!ready_) {
    return Status::kNotInit;
  }
  const Status status = common_.Write8(kDeviceStatus, value);
  if (IsErr(status)) {
    return status;
  }
  status_ = value;
  return Status::kOk;
}

Result<uint8_t> PciTransport::read_status() noexcept {
  if (!ready_) {
    return Result<uint8_t>::Err(Status::kNotInit);
  }
  return common_.Read8(kDeviceStatus);
}

Status PciTransport::reset() noexcept {
  status_ = 0;
  return write_status(0);
}

Status PciTransport::begin_init() noexcept {
  Status status = write_status(kStatusAcknowledge);
  if (IsErr(status)) {
    return status;
  }
  return write_status(kStatusAcknowledge | kStatusDriver);
}

Status PciTransport::negotiate(uint64_t desired_features) noexcept {
  if (!ready_) {
    return Status::kNotInit;
  }

  uint64_t device_features = 0;
  Status status = common_.Write32(kDeviceFeatureSelect, 0);
  if (IsErr(status)) {
    return status;
  }
  const Result<uint32_t> low = common_.Read32(kDeviceFeature);
  if (low.is_err()) {
    return low.status();
  }
  status = common_.Write32(kDeviceFeatureSelect, 1);
  if (IsErr(status)) {
    return status;
  }
  const Result<uint32_t> high = common_.Read32(kDeviceFeature);
  if (high.is_err()) {
    return high.status();
  }
  device_features = static_cast<uint64_t>(low.value());
  device_features |= static_cast<uint64_t>(high.value()) << 32U;

  const uint64_t agreed = device_features & desired_features;

  status = common_.Write32(kDriverFeatureSelect, 0);
  if (IsErr(status)) {
    return status;
  }
  status = common_.Write32(kDriverFeature, static_cast<uint32_t>(agreed & 0xFFFFFFFFULL));
  if (IsErr(status)) {
    return status;
  }
  status = common_.Write32(kDriverFeatureSelect, 1);
  if (IsErr(status)) {
    return status;
  }
  status = common_.Write32(kDriverFeature, static_cast<uint32_t>(agreed >> 32U));
  if (IsErr(status)) {
    return status;
  }

  status = write_status(static_cast<uint8_t>(status_ | kStatusFeaturesOk));
  if (IsErr(status)) {
    return status;
  }
  const Result<uint8_t> current = read_status();
  if (current.is_err()) {
    return current.status();
  }
  if ((current.value() & kStatusFeaturesOk) == 0U) {
    return Status::kUnsupported;
  }
  return Status::kOk;
}

Result<uint16_t> PciTransport::select_queue(uint16_t index) noexcept {
  if (!ready_) {
    return Result<uint16_t>::Err(Status::kNotInit);
  }
  const Status status = common_.Write16(kQueueSelect, index);
  if (IsErr(status)) {
    return Result<uint16_t>::Err(status);
  }
  return common_.Read16(kQueueSize);
}

Status PciTransport::configure_queue(uint16_t index, uint64_t descriptor_address,
                                     uint64_t available_address,
                                     uint64_t used_address) noexcept {
  if (!ready_) {
    return Status::kNotInit;
  }
  const Status select = common_.Write16(kQueueSelect, index);
  if (IsErr(select)) {
    return select;
  }

  Status status = common_.Write64(kQueueDescriptor, descriptor_address);
  if (IsErr(status)) {
    return status;
  }
  status = common_.Write64(kQueueAvailable, available_address);
  if (IsErr(status)) {
    return status;
  }
  status = common_.Write64(kQueueUsed, used_address);
  if (IsErr(status)) {
    return status;
  }

  const Result<uint16_t> notify = common_.Read16(kQueueNotifyOffset);
  if (notify.is_err()) {
    return notify.status();
  }
  notify_offset_ = notify.value();

  return common_.Write16(kQueueEnable, 1);
}

Status PciTransport::set_driver_ok() noexcept {
  return write_status(static_cast<uint8_t>(status_ | kStatusDriverOk));
}

Status PciTransport::notify(uint16_t index) noexcept {
  if (!ready_) {
    return Status::kNotInit;
  }
  const Result<uint16_t> notify_offset = common_.Read16(kQueueNotifyOffset);
  if (notify_offset.is_err()) {
    return notify_offset.status();
  }
  notify_offset_ = notify_offset.value();
  const uint64_t offset = static_cast<uint64_t>(notify_offset_) * notify_multiplier_;
  if (offset >= notify_.size()) {
    return Status::kInvalidArg;
  }
  return notify_.Write16(offset, index);
}

}  // namespace og::drivers::virtio
