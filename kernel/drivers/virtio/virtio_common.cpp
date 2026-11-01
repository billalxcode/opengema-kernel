#include "og/drivers/virtio.h"

#include "og/arch/registers.h"
#include "og/mem/vmm.h"

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

constexpr uint16_t kDescriptorNext = 1U << 0U;
constexpr uint16_t kDescriptorWrite = 1U << 1U;

struct Descriptor {
  uint64_t address;
  uint32_t length;
  uint16_t flags;
  uint16_t next;
};

static_assert(sizeof(Descriptor) == 16);

struct AvailableRing {
  uint16_t flags;
  uint16_t index;
};

struct UsedElement {
  uint32_t id;
  uint32_t length;
};

struct UsedRing {
  uint16_t flags;
  uint16_t index;
};

[[nodiscard]] uint64_t AlignUp(uint64_t value, uint64_t alignment) noexcept {
  return (value + alignment - 1U) & ~(alignment - 1U);
}

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

Status Queue::init(PciTransport& transport, uint16_t index,
                   uint16_t max_size) noexcept {
  if (ready_) {
    return Status::kOk;
  }
  if (!transport.is_ready()) {
    return Status::kNotInit;
  }
  if (max_size == 0) {
    return Status::kInvalidArg;
  }

  const Result<uint16_t> device_size = transport.select_queue(index);
  if (device_size.is_err()) {
    return device_size.status();
  }
  if (device_size.value() == 0) {
    return Status::kNoDevice;
  }
  if (device_size.value() > max_size) {
    return Status::kUnsupported;
  }
  size_ = device_size.value();
  index_ = index;

  avail_offset_ = AlignUp(16ULL * size_, 16);
  const uint64_t available_bytes = 6ULL + (2ULL * size_);
  used_offset_ = AlignUp(avail_offset_ + available_bytes, 8);
  used_bytes_ = 6ULL + (8ULL * size_);
  const uint64_t total = used_offset_ + used_bytes_;

  const Result<dma::DmaBuffer> memory = dma::AllocCoherent(total, 4096);
  if (memory.is_err()) {
    return memory.status();
  }
  memory_ = memory.value();

  const Status status = transport.configure_queue(
    index, memory_.device_address, memory_.device_address + avail_offset_,
    memory_.device_address + used_offset_);
  if (IsErr(status)) {
    return status;
  }

  transport_ = &transport;
  ready_ = true;
  return Status::kOk;
}

Status Queue::submit(const QueueBuffer* buffers, uint32_t count) noexcept {
  if (!ready_ || transport_ == nullptr) {
    return Status::kNotInit;
  }
  if (buffers == nullptr) {
    return Status::kInvalidArg;
  }
  if (count == 0 || count > kMaxChain) {
    return Status::kInvalidArg;
  }
  if (pending_count_ != 0) {
    return Status::kBusy;
  }

  auto* descriptors = static_cast<Descriptor*>(memory_.cpu_address);
  for (uint32_t index = 0; index < count; ++index) {
    if (buffers[index].cpu_address == nullptr || buffers[index].length == 0) {
      return Status::kInvalidArg;
    }
    descriptors[index].address = buffers[index].device_address;
    descriptors[index].length = buffers[index].length;
    descriptors[index].flags = 0;
    if ((index + 1U) < count) {
      descriptors[index].flags |= kDescriptorNext;
      descriptors[index].next = static_cast<uint16_t>(index + 1U);
    } else {
      descriptors[index].next = 0;
    }
    if (buffers[index].device_writes) {
      descriptors[index].flags |= kDescriptorWrite;
    }
  }

  const uint64_t available_base =
    reinterpret_cast<uint64_t>(memory_.cpu_address) + avail_offset_;
  auto* available = reinterpret_cast<AvailableRing*>(available_base);
  auto* ring = reinterpret_cast<uint16_t*>(available_base + 4);
  ring[avail_index_ % size_] = 0;
  __atomic_store_n(&available->index, static_cast<uint16_t>(avail_index_ + 1U),
                   __ATOMIC_RELEASE);
  ++avail_index_;

  Status status = dma::SyncForDevice(memory_.cpu_address, used_offset_);
  if (IsErr(status)) {
    return status;
  }
  for (uint32_t index = 0; index < count; ++index) {
    status = dma::SyncForDevice(buffers[index].cpu_address, buffers[index].length);
    if (IsErr(status)) {
      return status;
    }
    pending_[index] = buffers[index];
  }
  pending_count_ = count;
  used_length_ = 0;

  return transport_->notify(index_);
}

Status Queue::wait() noexcept {
  if (!ready_) {
    return Status::kNotInit;
  }
  if (pending_count_ == 0) {
    return Status::kInvalidArg;
  }

  const uint64_t timeout_ticks = arch::aarch64::ReadCntfrqEl0();
  const uint64_t start = arch::aarch64::ReadCntpctEl0();
  const uint64_t used_base =
    reinterpret_cast<uint64_t>(memory_.cpu_address) + used_offset_;

  for (;;) {
    Status status = dma::SyncForCpu(reinterpret_cast<void*>(used_base), used_bytes_);
    if (IsErr(status)) {
      return status;
    }
    auto* used = reinterpret_cast<UsedRing*>(used_base);
    const uint16_t current = __atomic_load_n(&used->index, __ATOMIC_ACQUIRE);
    if (current != used_index_) {
      auto* element = reinterpret_cast<UsedElement*>(
        used_base + 4U + (static_cast<uint64_t>(used_index_ % size_) * 8U));
      used_length_ = element->length;
      used_index_ = current;
      break;
    }
    if ((arch::aarch64::ReadCntpctEl0() - start) > timeout_ticks) {
      return Status::kTimeout;
    }
  }

  for (uint32_t index = 0; index < pending_count_; ++index) {
    const Status status =
      dma::SyncForCpu(pending_[index].cpu_address, pending_[index].length);
    if (IsErr(status)) {
      return status;
    }
  }
  pending_count_ = 0;
  return Status::kOk;
}

}  // namespace og::drivers::virtio
