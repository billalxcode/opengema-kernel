#include "og/drivers/virtio_gpu.h"

#include "memory.h"
#include "og/dma/dma.h"

namespace og::drivers::virtio {

namespace {

constexpr uint32_t kRequestMemorySize = 4096;
constexpr uint32_t kResponseMemorySize = 4096;
constexpr uint32_t kDefaultWidth = 1024;
constexpr uint32_t kDefaultHeight = 768;
constexpr uint32_t kMaxWidth = 1920;
constexpr uint32_t kMaxHeight = 1200;
constexpr uint32_t kBytesPerPixel = 4;
constexpr uint32_t kScanoutId = 0;
constexpr uint32_t kMaxScanouts = 16;

constexpr uint16_t kVirtioDeviceIdLegacyGpu = 0x1010;
constexpr uint16_t kVirtioDeviceIdModernGpu = 0x1050;

constexpr uint32_t kCmdGetDisplayInfo = 0x0100;
constexpr uint32_t kCmdResourceCreate2d = 0x0101;
constexpr uint32_t kCmdSetScanout = 0x0103;
constexpr uint32_t kCmdResourceFlush = 0x0104;
constexpr uint32_t kCmdTransferToHost2d = 0x0105;
constexpr uint32_t kCmdResourceAttachBacking = 0x0106;

constexpr uint32_t kRespOkNoData = 0x1100;
constexpr uint32_t kRespOkDisplayInfo = 0x1101;

constexpr uint32_t kFormatB8G8R8A8Unorm = 1;
constexpr uint32_t kDeviceConfigScanouts = 8;

struct CtrlHeader {
  uint32_t type;
  uint32_t flags;
  uint64_t fence_id;
  uint32_t ctx_id;
  uint32_t padding;
};

static_assert(sizeof(CtrlHeader) == 24);

struct Rect {
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
};

static_assert(sizeof(Rect) == 16);

struct DisplayOne {
  Rect rect;
  uint32_t enabled;
  uint32_t flags;
};

static_assert(sizeof(DisplayOne) == 24);

struct DisplayInfo {
  CtrlHeader header;
  DisplayOne modes[kMaxScanouts];
};

static_assert(sizeof(DisplayInfo) == 24 + (kMaxScanouts * 24));

struct ResourceCreate2d {
  CtrlHeader header;
  uint32_t resource_id;
  uint32_t format;
  uint32_t width;
  uint32_t height;
};

static_assert(sizeof(ResourceCreate2d) == 40);

struct MemEntry {
  uint64_t address;
  uint32_t length;
  uint32_t padding;
};

static_assert(sizeof(MemEntry) == 16);

struct AttachBacking {
  CtrlHeader header;
  uint32_t resource_id;
  uint32_t entry_count;
  MemEntry entry;
};

static_assert(sizeof(AttachBacking) == 48);

struct SetScanout {
  CtrlHeader header;
  Rect rect;
  uint32_t scanout_id;
  uint32_t resource_id;
};

static_assert(sizeof(SetScanout) == 48);

struct TransferToHost2d {
  CtrlHeader header;
  Rect rect;
  uint64_t offset;
  uint32_t resource_id;
  uint32_t padding;
};

static_assert(sizeof(TransferToHost2d) == 56);

struct ResourceFlush {
  CtrlHeader header;
  Rect rect;
  uint32_t resource_id;
  uint32_t padding;
};

static_assert(sizeof(ResourceFlush) == 48);

}  // namespace

bool IsVirtioGpu(const pci::Info& info) noexcept {
  if (info.vendor_id != pci::kVendorRedHat) {
    return false;
  }
  return info.device_id == kVirtioDeviceIdLegacyGpu ||
         info.device_id == kVirtioDeviceIdModernGpu;
}

Status Gpu::command(const void* request, uint32_t request_size,
                    uint32_t expected_response, void* response,
                    uint32_t response_size) noexcept {
  if (request == nullptr || request_size < sizeof(CtrlHeader)) {
    return Status::kInvalidArg;
  }
  if (request_size > request_memory_.size || response_size > response_memory_.size) {
    return Status::kInvalidArg;
  }

  mem::Copy(request_memory_.cpu_address, request, request_size);
  mem::Zero(response_memory_.cpu_address, response_size);

  const QueueBuffer buffers[2] = {
    QueueBuffer{request_memory_.cpu_address, request_memory_.device_address, request_size, false},
    QueueBuffer{response_memory_.cpu_address, response_memory_.device_address, response_size, true},
  };
  Status status = queue_.submit(buffers, 2);
  if (IsErr(status)) {
    return status;
  }
  status = queue_.wait();
  if (IsErr(status)) {
    return status;
  }
  if (queue_.used_length() < sizeof(CtrlHeader)) {
    return Status::kIoError;
  }

  const auto* header = static_cast<const CtrlHeader*>(response_memory_.cpu_address);
  if (header->type != expected_response) {
    return Status::kIoError;
  }
  if (response != nullptr) {
    mem::Copy(response, response_memory_.cpu_address, response_size);
  }
  return Status::kOk;
}

Status Gpu::create_resource() noexcept {
  ResourceCreate2d request{};
  request.header.type = kCmdResourceCreate2d;
  request.resource_id = resource_id_;
  request.format = kFormatB8G8R8A8Unorm;
  request.width = width_;
  request.height = height_;
  return command(&request, sizeof(request), kRespOkNoData, nullptr, sizeof(CtrlHeader));
}

Status Gpu::attach_backing() noexcept {
  AttachBacking request{};
  request.header.type = kCmdResourceAttachBacking;
  request.resource_id = resource_id_;
  request.entry_count = 1;
  request.entry.address = framebuffer_.device_address;
  request.entry.length = static_cast<uint32_t>(stride_) * height_;
  return command(&request, sizeof(request), kRespOkNoData, nullptr, sizeof(CtrlHeader));
}

Status Gpu::set_scanout() noexcept {
  SetScanout request{};
  request.header.type = kCmdSetScanout;
  request.rect.width = width_;
  request.rect.height = height_;
  request.scanout_id = kScanoutId;
  request.resource_id = resource_id_;
  return command(&request, sizeof(request), kRespOkNoData, nullptr, sizeof(CtrlHeader));
}

Status Gpu::init(const pci::Info& info) noexcept {
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
  status = queue_.init(transport_, 0, 1024);
  if (IsErr(status)) {
    return status;
  }
  status = transport_.set_driver_ok();
  if (IsErr(status)) {
    return status;
  }

  const Result<uint32_t> scanouts =
    transport_.device_config().Read32(kDeviceConfigScanouts);
  if (scanouts.is_err()) {
    return scanouts.status();
  }
  num_scanouts_ = scanouts.value();
  if (num_scanouts_ == 0) {
    return Status::kNoDevice;
  }

  const Result<dma::DmaBuffer> request =
    dma::AllocCoherent(kRequestMemorySize, 4096);
  if (request.is_err()) {
    return request.status();
  }
  request_memory_ = request.value();

  const Result<dma::DmaBuffer> response =
    dma::AllocCoherent(kResponseMemorySize, 4096);
  if (response.is_err()) {
    return response.status();
  }
  response_memory_ = response.value();

  CtrlHeader info_request{};
  info_request.type = kCmdGetDisplayInfo;
  DisplayInfo display{};
  status = command(&info_request, sizeof(info_request), kRespOkDisplayInfo, &display,
                   sizeof(display));
  if (IsErr(status)) {
    return status;
  }

  uint32_t width = 0;
  uint32_t height = 0;
  for (uint32_t index = 0; index < kMaxScanouts; ++index) {
    if (display.modes[index].enabled != 0U && display.modes[index].rect.width != 0U &&
        display.modes[index].rect.height != 0U) {
      width = display.modes[index].rect.width;
      height = display.modes[index].rect.height;
      break;
    }
  }
  if (width == 0 || height == 0) {
    for (uint32_t index = 0; index < kMaxScanouts; ++index) {
      if (display.modes[index].rect.width != 0U && display.modes[index].rect.height != 0U) {
        width = display.modes[index].rect.width;
        height = display.modes[index].rect.height;
        break;
      }
    }
  }
  if (width == 0 || height == 0) {
    width = kDefaultWidth;
    height = kDefaultHeight;
  }
  if (width > kMaxWidth || height > kMaxHeight) {
    return Status::kUnsupported;
  }

  width_ = width;
  height_ = height;
  stride_ = width_ * kBytesPerPixel;

  const Result<dma::DmaBuffer> framebuffer =
    dma::AllocCoherent(static_cast<uint64_t>(stride_) * height_, 4096);
  if (framebuffer.is_err()) {
    return framebuffer.status();
  }
  framebuffer_ = framebuffer.value();

  status = create_resource();
  if (IsErr(status)) {
    return status;
  }
  status = attach_backing();
  if (IsErr(status)) {
    return status;
  }
  status = set_scanout();
  if (IsErr(status)) {
    return status;
  }

  status = console_.init(framebuffer_.cpu_address, width_, height_, stride_,
                         io::Rgb(232, 234, 240), io::Rgb(16, 20, 28),
                         &PresentTrampoline, this);
  if (IsErr(status)) {
    return status;
  }

  ready_ = true;
  status = console_.present();
  if (IsErr(status)) {
    ready_ = false;
    return status;
  }
  return Status::kOk;
}

Status Gpu::flush(uint32_t x, uint32_t y, uint32_t width, uint32_t height) noexcept {
  if (framebuffer_.cpu_address == nullptr) {
    return Status::kNotInit;
  }
  if (x >= width_ || y >= height_ || width == 0 || height == 0) {
    return Status::kInvalidArg;
  }
  if (width > (width_ - x)) {
    width = width_ - x;
  }
  if (height > (height_ - y)) {
    height = height_ - y;
  }

  const Status sync = dma::SyncForDevice(
    static_cast<uint8_t*>(framebuffer_.cpu_address) +
      (static_cast<std::size_t>(y) * stride_),
    static_cast<uint64_t>(height) * stride_);
  if (IsErr(sync)) {
    return sync;
  }

  TransferToHost2d transfer{};
  transfer.header.type = kCmdTransferToHost2d;
  transfer.rect.x = x;
  transfer.rect.y = y;
  transfer.rect.width = width;
  transfer.rect.height = height;
  transfer.offset = (static_cast<uint64_t>(y) * stride_) +
                    (static_cast<uint64_t>(x) * kBytesPerPixel);
  transfer.resource_id = resource_id_;
  Status status = command(&transfer, sizeof(transfer), kRespOkNoData, nullptr,
                          sizeof(CtrlHeader));
  if (IsErr(status)) {
    return status;
  }

  ResourceFlush flush{};
  flush.header.type = kCmdResourceFlush;
  flush.rect = transfer.rect;
  flush.resource_id = resource_id_;
  return command(&flush, sizeof(flush), kRespOkNoData, nullptr, sizeof(CtrlHeader));
}

Status Gpu::PresentTrampoline(void* context, uint32_t x, uint32_t y,
                              uint32_t width, uint32_t height) noexcept {
  if (context == nullptr) {
    return Status::kInvalidArg;
  }
  return static_cast<Gpu*>(context)->flush(x, y, width, height);
}

}  // namespace og::drivers::virtio
