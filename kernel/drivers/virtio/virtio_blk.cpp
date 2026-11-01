#include "og/drivers/virtio_blk.h"

#include "memory.h"
#include "og/arch/spinlock.h"
#include "og/dma/dma.h"

namespace og::drivers::virtio {

namespace {

constexpr uint64_t kBlockSize = 512;
constexpr uint64_t kMaxTransfer = 4096;
constexpr uint64_t kRequestHeaderSize = 16;
constexpr uint64_t kRequestStatusOffset = 16;
constexpr uint64_t kRequestDataOffset = 64;
constexpr uint64_t kRequestMemorySize = 8192;

constexpr uint32_t kRequestTypeIn = 0;

constexpr uint16_t kVirtioDeviceIdLegacyBlock = 0x1001;
constexpr uint16_t kVirtioDeviceIdModernBlock = 0x1042;

struct RequestHeader {
  uint32_t type;
  uint32_t reserved;
  uint64_t sector;
};

static_assert(sizeof(RequestHeader) == 16);

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

  const Result<dma::DmaBuffer> request = dma::AllocCoherent(kRequestMemorySize, 4096);
  if (request.is_err()) {
    return request.status();
  }
  request_memory_ = request.value();

  status = queue_.init(transport_, 0, 1024);
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

  // One queue and one request buffer are shared by all CPUs. Serialize the
  // whole request so a fork or loader read cannot overwrite an in-flight
  // descriptor.
  arch::aarch64::LockGuard<arch::aarch64::IrqSpinlock> guard(lock_);

  auto* request_base = static_cast<uint8_t*>(request_memory_.cpu_address);
  auto* header = reinterpret_cast<RequestHeader*>(request_base);
  header->type = kRequestTypeIn;
  header->reserved = 0;
  header->sector = sector;
  request_base[kRequestStatusOffset] = 0xFFU;

  const QueueBuffer buffers[3] = {
    QueueBuffer{request_base, request_memory_.device_address,
                static_cast<uint32_t>(kRequestHeaderSize), false},
    QueueBuffer{request_base + kRequestDataOffset,
                request_memory_.device_address + kRequestDataOffset,
                static_cast<uint32_t>(buffer.size()), true},
    QueueBuffer{request_base + kRequestStatusOffset,
                request_memory_.device_address + kRequestStatusOffset, 1, true},
  };

  Status status = queue_.submit(buffers, 3);
  if (IsErr(status)) {
    return status;
  }
  status = queue_.wait();
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
