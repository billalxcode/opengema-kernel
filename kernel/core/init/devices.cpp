#include "boot.h"

#include "memory.h"
#include "og/core/log.h"
#include "og/core/status.h"
#include "og/drivers/device.h"
#include "og/drivers/pci.h"
#include "og/drivers/rtc.h"
#include "og/drivers/virtio_blk.h"
#include "og/drivers/virtio_gpu.h"
#include "og/io/console.h"
#include "og/platform/platform.h"

namespace og::boot {

namespace {

constexpr uint64_t kSectorSize = 512;

drivers::virtio::Blk g_block_device;
drivers::virtio::Gpu g_gpu;
drivers::rtc::Pl031 g_rtc;
io::Tee g_console_tee;

void InitRtc() noexcept {
  const platform::Platform& board = platform::Current();
  const Status status = g_rtc.init(board.rtc.base, board.rtc.size);
  if (IsErr(status)) {
    Log(LogLevel::kWarn, "rtc: init failed\n");
    return;
  }
  const Result<uint64_t> seconds = g_rtc.read_unix_seconds();
  if (seconds.is_err()) {
    Log(LogLevel::kWarn, "rtc: read failed\n");
    return;
  }
  LogDec(LogLevel::kInfo, "rtc: unix seconds", seconds.value());

  drivers::Device device{};
  device.name = "pl031";
  device.device_class = drivers::DeviceClass::kUnknown;
  device.capacity = seconds.value();
  device.driver_data = &g_rtc;
  (void)drivers::RegisterDevice(device);
}

void InitBlockDevice() noexcept {
  Status status = drivers::DeviceInit();
  if (IsErr(status)) {
    Log(LogLevel::kError, "device: registry init failed\n");
    return;
  }

  status = drivers::pci::PciInit(platform::Current().pci_ecam.base,
                                platform::Current().pci_ecam.size);
  if (IsErr(status)) {
    Log(LogLevel::kError, "pci: ecam init failed\n");
    return;
  }

  Result<drivers::pci::Info> info = drivers::pci::FindFirst(&drivers::virtio::IsVirtioBlock);
  if (info.is_err()) {
    Log(LogLevel::kWarn, "virtio-blk: no device on pci\n");
    return;
  }
  LogDec(LogLevel::kInfo, "pci: virtio device id", info.value().device_id);

  status = drivers::pci::EnableDevice(info.value().address);
  if (IsErr(status)) {
    Log(LogLevel::kError, "pci: enable device failed\n");
    return;
  }

  status = g_block_device.init(info.value());
  if (IsErr(status)) {
    Log(LogLevel::kError, "virtio-blk: init failed\n");
    return;
  }
  LogDec(LogLevel::kInfo, "virtio-blk: queue size", g_block_device.queue_size());
  LogDec(LogLevel::kInfo, "virtio-blk: capacity sectors", g_block_device.capacity_sectors());

  drivers::Device device{};
  device.name = "virtio-blk0";
  device.device_class = drivers::DeviceClass::kBlock;
  device.capacity = g_block_device.capacity_sectors();
  device.driver_data = &g_block_device;
  status = drivers::RegisterDevice(device);
  if (IsErr(status)) {
    Log(LogLevel::kError, "device: register failed\n");
    return;
  }

  uint8_t sector[kSectorSize];
  status = g_block_device.read(1, std::span<uint8_t>(sector, kSectorSize));
  if (IsErr(status)) {
    Log(LogLevel::kError, "virtio-blk: read failed\n");
    return;
  }
  if (!mem::Equal(sector, "EFI PART", 8)) {
    Log(LogLevel::kError, "virtio-blk: gpt signature mismatch\n");
    return;
  }
  Log(LogLevel::kInfo, "virtio-blk: gpt header ok\n");
}

void InitGpu() noexcept {
  if (!drivers::pci::PciReady()) {
    Log(LogLevel::kWarn, "virtio-gpu: pci not ready\n");
    return;
  }

  const Result<drivers::pci::Info> info =
    drivers::pci::FindFirst(&drivers::virtio::IsVirtioGpu);
  if (info.is_err()) {
    Log(LogLevel::kWarn, "virtio-gpu: no device on pci\n");
    return;
  }
  LogDec(LogLevel::kInfo, "pci: virtio-gpu device id", info.value().device_id);

  Status status = drivers::pci::EnableDevice(info.value().address);
  if (IsErr(status)) {
    Log(LogLevel::kError, "virtio-gpu: pci enable failed\n");
    return;
  }

  status = g_gpu.init(info.value());
  if (IsErr(status)) {
    LogHex(LogLevel::kError, "virtio-gpu: init status", static_cast<uint64_t>(status));
    return;
  }
  LogDec(LogLevel::kInfo, "virtio-gpu: display width", g_gpu.width());
  LogDec(LogLevel::kInfo, "virtio-gpu: display height", g_gpu.height());
  LogDec(LogLevel::kInfo, "virtio-gpu: scanouts", g_gpu.num_scanouts());

  drivers::Device device{};
  device.name = "virtio-gpu0";
  device.device_class = drivers::DeviceClass::kDisplay;
  device.capacity = static_cast<uint64_t>(g_gpu.width()) * g_gpu.height();
  device.driver_data = &g_gpu;
  status = drivers::RegisterDevice(device);
  if (IsErr(status)) {
    Log(LogLevel::kError, "virtio-gpu: register failed\n");
    return;
  }

  g_console_tee.Set(io::ActiveConsole(), g_gpu.console());
  io::SetConsole(g_console_tee.console());
  Log(LogLevel::kInfo, "virtio-gpu: scanout ok\n");
}

}  // namespace

void InitDevices() noexcept {
  InitBlockDevice();
  InitGpu();
  InitRtc();
}

}  // namespace og::boot
