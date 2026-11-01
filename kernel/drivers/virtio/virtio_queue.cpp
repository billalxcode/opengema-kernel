#include "og/drivers/virtio.h"

#include "og/arch/registers.h"
#include "og/drivers/virtio_internal.h"

namespace og::drivers::virtio {

using namespace detail;

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

Status Queue::submit_each(const QueueBuffer* buffers, uint32_t count) noexcept {
  if (!ready_ || transport_ == nullptr) {
    return Status::kNotInit;
  }
  if (buffers == nullptr) {
    return Status::kInvalidArg;
  }
  if (count == 0 || count > kMaxBuffers) {
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
    descriptors[index].flags =
      buffers[index].device_writes ? kDescriptorWrite : 0;
    descriptors[index].next = 0;
  }

  const uint64_t available_base =
    reinterpret_cast<uint64_t>(memory_.cpu_address) + avail_offset_;
  auto* available = reinterpret_cast<AvailableRing*>(available_base);
  auto* ring = reinterpret_cast<uint16_t*>(available_base + 4);
  for (uint32_t index = 0; index < count; ++index) {
    ring[(avail_index_ + index) % size_] = static_cast<uint16_t>(index);
  }
  __atomic_store_n(&available->index,
                   static_cast<uint16_t>(avail_index_ + count),
                   __ATOMIC_RELEASE);
  avail_index_ = static_cast<uint16_t>(avail_index_ + count);

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

Status Queue::poll(bool& ready) noexcept {
  ready = false;
  if (!ready_ || pending_count_ == 0U) {
    return Status::kNotInit;
  }

  const uint64_t used_base =
    reinterpret_cast<uint64_t>(memory_.cpu_address) + used_offset_;
  Status status =
    dma::SyncForCpu(reinterpret_cast<void*>(used_base), used_bytes_);
  if (IsErr(status)) {
    return status;
  }
  auto* used = reinterpret_cast<UsedRing*>(used_base);
  const uint16_t current = __atomic_load_n(&used->index, __ATOMIC_ACQUIRE);
  const uint32_t consumed =
    static_cast<uint16_t>(current - used_index_);
  if (consumed == 0U) {
    return Status::kOk;
  }

  auto* element = reinterpret_cast<UsedElement*>(
    used_base + 4U + (static_cast<uint64_t>(used_index_ % size_) * 8U));
  used_length_ = element->length;
  used_index_ = current;

  for (uint32_t index = 0; index < pending_count_; ++index) {
    status = dma::SyncForCpu(pending_[index].cpu_address, pending_[index].length);
    if (IsErr(status)) {
      return status;
    }
  }
  if (pending_count_ >= consumed) {
    pending_count_ -= consumed;
  } else {
    pending_count_ = 0;
  }
  ready = true;
  return Status::kOk;
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
    Status status =
      dma::SyncForCpu(reinterpret_cast<void*>(used_base), used_bytes_);
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
