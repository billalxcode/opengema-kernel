#include "og/arch/cpu.h"
#include "og/arch/registers.h"

#include "memory.h"
#include "og/bootinfo.h"
#include "og/device.h"
#include "og/dma.h"
#include "og/dma_selftest.h"
#include "og/event_selftest.h"
#include "og/gic.h"
#include "og/heap.h"
#include "og/irq.h"
#include "og/log.h"
#include "og/memory_map.h"
#include "og/mem_selftest.h"
#include "og/panic.h"
#include "og/pci.h"
#include "og/pmm.h"
#include "og/print.h"
#include "og/status.h"
#include "og/timer.h"
#include "og/trap.h"
#include "og/virtio_blk.h"
#include "og/vmm.h"
#include "og/workqueue.h"

namespace og {

namespace {

constexpr uint32_t kTickHz = 100;
constexpr uint64_t kTargetTicks = 100;
constexpr uint64_t kSectorSize = 512;
uint64_t g_work_runs = 0;
drivers::virtio::Blk g_block_device;

void CountWork(void* /*context*/) noexcept {
  ++g_work_runs;
}

void InitBlockDevice() noexcept {
  Status status = drivers::DeviceInit();
  if (IsErr(status)) {
    Log(LogLevel::kError, "device: registry init failed\n");
    return;
  }

  status = drivers::pci::PciInit(memmap::kPciEcamBase, memmap::kPciEcamSize);
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

}  // namespace

// Single entry point after the firmware hands over control. Modules are
// initialized in dependency order and the first failure stops the chain.
void kmain(BootInfo* info) noexcept {
  if (info == nullptr) {
    Panic("kmain: null boot info");
  }
  if (info->magic != kBootInfoMagic) {
    Panic("kmain: bad boot info magic");
  }
  if (info->version != kBootInfoVersion) {
    Panic("kmain: unsupported boot info version");
  }

  // Install vectors first so any fault after this point panics with context.
  events::InstallVectorTable();

  Log(LogLevel::kInfo, "kernel: kmain entered\n");
  LogDec(LogLevel::kInfo, "current el", arch::aarch64::CurrentEl());
  LogDec(LogLevel::kInfo, "cpu id", arch::aarch64::CpuId());
  (void)PrintHex("uart base", info->uart_base);

  Status status = mem::PmmInit(*info);
  if (IsErr(status)) {
    Panic("mem: pmm init failed");
  }
  LogDec(LogLevel::kInfo, "pmm: total frames", mem::PmmTotalFrames());
  LogDec(LogLevel::kInfo, "pmm: free frames", mem::PmmFreeFrames());

  status = mem::VmmInit(*info);
  if (IsErr(status)) {
    Panic("mem: vmm init failed");
  }
  Log(LogLevel::kInfo, "vmm: identity map active\n");

  status = mem::HeapInit();
  if (IsErr(status)) {
    Panic("mem: heap init failed");
  }

  status = mem::RunSelfTest();
  if (IsErr(status)) {
    Panic("mem: self test failed");
  }

  status = dma::RunDmaSelfTest();
  if (IsErr(status)) {
    Panic("dma: self test failed");
  }

  InitBlockDevice();

  status = events::WorkQueueInit();
  if (IsErr(status)) {
    Panic("events: workqueue init failed");
  }
  status = events::RunEventSelfTest();
  if (IsErr(status)) {
    Panic("events: self test failed");
  }

  drivers::irq::Gicv3 gic;
  status = gic.init(memmap::kGicDistBase, memmap::kGicRedistBase);
  if (IsErr(status)) {
    Panic("gic: init failed");
  }
  events::SetInterruptController(drivers::irq::MakeController(gic));
  Log(LogLevel::kInfo, "gic: ready\n");

  status = time::TimerInit();
  if (IsErr(status)) {
    Panic("timer: init failed");
  }
  status = gic.enable_irq(time::kTimerIrq);
  if (IsErr(status)) {
    Panic("timer: irq enable failed");
  }
  LogDec(LogLevel::kInfo, "timer: frequency", time::TimerFrequency());

  status = time::TimerStartPeriodic(kTickHz);
  if (IsErr(status)) {
    Panic("timer: start failed");
  }

  (void)events::ScheduleWork(&CountWork, nullptr);
  arch::aarch64::EnableIrq();

  // Bounded wait so a missing interrupt reports instead of hanging.
  const uint64_t start = arch::aarch64::ReadCntpctEl0();
  const uint64_t timeout = arch::aarch64::ReadCntfrqEl0() * 2ULL;
  while (time::TimerTicks() < kTargetTicks) {
    if ((arch::aarch64::ReadCntpctEl0() - start) > timeout) {
      break;
    }
  }
  arch::aarch64::DisableIrq();

  const uint64_t ticks = time::TimerTicks();
  LogDec(LogLevel::kInfo, "timer: ticks", ticks);
  LogDec(LogLevel::kInfo, "timer: work runs", g_work_runs);
  if (ticks < kTargetTicks) {
    Log(LogLevel::kError, "timer: no interrupts received\n");
    Panic("timer: irq not working");
  }
  Log(LogLevel::kInfo, "timer: periodic tick ok\n");

  (void)PrintLn("kernel: halt");
  arch::aarch64::HaltForever();
}

}  // namespace og
