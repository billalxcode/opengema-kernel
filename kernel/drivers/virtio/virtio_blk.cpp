#include "og/virtio_blk.h"

#include "memory.h"
#include "og/arch/registers.h"
#include "og/dma.h"

namespace og::drivers::virtio {

namespace {

constexpr uint64_t kBlockSize = 512;
constexpr uint64_t kMaxTransfer = 4096;
constexpr uint64_t kRequestHeaderSize = 16;
constexpr uint64_t kRequestStatusOffset = 16;
constexpr uint64_t kRequestDataOffset = 64;
constexpr uint64_t kRequestMemorySize = 8192;

constexpr uint32_t kRequestTypeIn = 0;
constexpr uint16_t kDescriptorNext = 1U << 0U;
constexpr uint16_t kDescriptorWrite = 1U << 1U;
constexpr uint64_t kFeatureVersion1 = 1ULL << 32U;

constexpr uint16_t kVirtioDeviceIdLegacyBlock = 0x1001;
constexpr uint16_t kVirtioDeviceIdModernBlock = 0x1042;

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

struct RequestHeader {
  uint32_t type;
  uint32_t reserved;
  uint64_t sector;
};

static_assert(sizeof(RequestHeader) == 16);

[[nodiscard]] uint64_t AlignUp(uint64_t value, uint64_t alignment) noexcept {
  return (value + alignment - 1U) & ~(alignment - 1U);
}

}  // namespace

bool IsVirtioBlock(const pci::Info& info) noexcept {
  if (info.vendor_id != pci::kVendorRedHat) {
    return false;
  }
  return info.device_id == kVirtioDeviceIdLegacyBlock ||
         info.device_id == kVirtioDeviceIdModernBlock;
}

Status Blk::init(const pci::Info& info) noexcept {
  if (ready_) {
    return Status::kOk;
  }

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

  const Result<uint16_t> size = transport_.select_queue(0);
  if (size.is_err()) {
    return size.status();
  }
  if (size.value() == 0) {
    return Status::kNoDevice;
  }
  queue_size_ = size.value();

  const uint64_t descriptor_bytes = 16ULL * queue_size_;
  avail_offset_ = AlignUp(descriptor_bytes, 16);
  const uint64_t available_bytes = 6ULL + (2ULL * queue_size_);
  used_offset_ = AlignUp(avail_offset_ + available_bytes, 8);
  used_bytes_ = 6ULL + (8ULL * queue_size_);
  const uint64_t total = used_offset_ + used_bytes_;

  Result<dma::DmaBuffer> queue = dma::AllocCoherent(total, 4096);
  if (queue.is_err()) {
    return queue.status();
  }
  queue_memory_ = queue.value();

  Result<dma::DmaBuffer> request = dma::AllocCoherent(kRequestMemorySize, 4096);
  if (request.is_err()) {
    (void)dma::FreeCoherent(queue_memory_);
    return request.status();
  }
  request_memory_ = request.value();

  status = transport_.configure_queue(0, queue_memory_.device_address,
                                      queue_memory_.device_address + avail_offset_,
                                      queue_memory_.device_address + used_offset_);
  if (IsErr(status)) {
    return status;
  }
  status = transport_.set_driver_ok();
  if (IsErr(status)) {
    return status;
  }

  const Result<uint64_t> capacity = transport_.device_config().Read64(0);
  if (capacity.is_err()) {
    return capacity.status();
  }
  capacity_ = capacity.value();
  ready_ = true;
  return Status::kOk;
}

Status Blk::read(uint64_t sector, std::span<uint8_t> buffer) noexcept {
  if (!ready_) {
    return Status::kNotInit;
  }
  if (buffer.empty()) {
    return Status::kInvalidArg;
  }
  if ((buffer.size() % kBlockSize) != 0U) {
    return Status::kInvalidArg;
  }
  if (buffer.size() > kMaxTransfer) {
    return Status::kInvalidArg;
  }
  const uint64_t sectors = buffer.size() / kBlockSize;
  if (sector > capacity_) {
    return Status::kInvalidArg;
  }
  if (sectors > (capacity_ - sector)) {
    return Status::kInvalidArg;
  }

  auto* request_base = static_cast<uint8_t*>(request_memory_.cpu_address);
  auto* header = reinterpret_cast<RequestHeader*>(request_base);
  header->type = kRequestTypeIn;
  header->reserved = 0;
  header->sector = sector;
  request_base[kRequestStatusOffset] = 0xFFU;

  auto* descriptors = static_cast<Descriptor*>(queue_memory_.cpu_address);
  descriptors[0].address = request_memory_.device_address;
  descriptors[0].length = static_cast<uint32_t>(kRequestHeaderSize);
  descriptors[0].flags = kDescriptorNext;
  descriptors[0].next = 1;
  descriptors[1].address = request_memory_.device_address + kRequestDataOffset;
  descriptors[1].length = static_cast<uint32_t>(buffer.size());
  descriptors[1].flags = static_cast<uint16_t>(kDescriptorNext | kDescriptorWrite);
  descriptors[1].next = 2;
  descriptors[2].address = request_memory_.device_address + kRequestStatusOffset;
  descriptors[2].length = 1;
  descriptors[2].flags = kDescriptorWrite;
  descriptors[2].next = 0;

  const uint64_t available_base = reinterpret_cast<uint64_t>(queue_memory_.cpu_address) +
                                  avail_offset_;
  auto* available = reinterpret_cast<AvailableRing*>(available_base);
  auto* available_ring = reinterpret_cast<uint16_t*>(available_base + 4);
  available_ring[avail_index_ % queue_size_] = 0;
  __atomic_store_n(&available->index, static_cast<uint16_t>(avail_index_ + 1U),
                   __ATOMIC_RELEASE);
  ++avail_index_;

  Status status = dma::SyncForDevice(queue_memory_.cpu_address, used_offset_);
  if (IsErr(status)) {
    return status;
  }
  status = transport_.notify(0);
  if (IsErr(status)) {
    return status;
  }

  const uint64_t timeout_ticks = arch::aarch64::ReadCntfrqEl0();
  const uint64_t start = arch::aarch64::ReadCntpctEl0();
  const uint64_t used_base = reinterpret_cast<uint64_t>(queue_memory_.cpu_address) + used_offset_;

  for (;;) {
    status = dma::SyncForCpu(reinterpret_cast<void*>(used_base), used_bytes_);
    if (IsErr(status)) {
      return status;
    }
    auto* used = reinterpret_cast<UsedRing*>(used_base);
    const uint16_t current = __atomic_load_n(&used->index, __ATOMIC_ACQUIRE);
    if (current != used_index_) {
      used_index_ = current;
      break;
    }
    if ((arch::aarch64::ReadCntpctEl0() - start) > timeout_ticks) {
      return Status::kTimeout;
    }
  }

  status = dma::SyncForCpu(request_memory_.cpu_address, kRequestDataOffset + buffer.size());
  if (IsErr(status)) {
    return status;
  }
  if (request_base[kRequestStatusOffset] != 0U) {
    return Status::kIoError;
  }

  mem::Copy(buffer.data(), request_base + kRequestDataOffset, buffer.size());
  return Status::kOk;
}

}  // namespace og::drivers::virtio
