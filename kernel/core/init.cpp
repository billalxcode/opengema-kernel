#include "og/arch/cpu.h"
#include "og/arch/registers.h"
#include "og/arch/user.h"

#include "memory.h"
#include "og/block_device.h"
#include "og/bootinfo.h"
#include "og/device.h"
#include "og/devfs.h"
#include "og/dma.h"
#include "og/dma_selftest.h"
#include "og/event_selftest.h"
#include "og/fat32.h"
#include "og/gic.h"
#include "og/heap.h"
#include "og/irq.h"
#include "og/log.h"
#include "og/memory_map.h"
#include "og/mem_selftest.h"
#include "og/panic.h"
#include "og/partition.h"
#include "og/pci.h"
#include "og/pmm.h"
#include "og/print.h"
#include "og/sched.h"
#include "og/status.h"
#include "og/syscall.h"
#include "og/timer.h"
#include "og/trap.h"
#include "og/vfs.h"
#include "og/virtio_blk.h"
#include "og/vmm.h"
#include "og/workqueue.h"

#include "user_program.h"
#include "og/workqueue.h"

namespace og {

namespace {

constexpr uint32_t kTickHz = 100;
// The host can starve the vCPU, so prove interrupts arrive without
// asserting a wall clock rate. Only zero ticks is treated as a failure.
constexpr uint64_t kTargetTicks = 10;
constexpr uint64_t kTimeoutSeconds = 10;
constexpr uint64_t kSectorSize = 512;
constexpr uint64_t kUserCodeVa = 0x100000000ULL;
constexpr uint64_t kUserStackVa = 0x100100000ULL;
constexpr uint64_t kUserStackSize = 16384;
uint64_t g_work_runs = 0;
uint64_t g_workers_done = 0;
drivers::virtio::Blk g_block_device;

void CountWork(void* /*context*/) noexcept {
  ++g_work_runs;
}

void WorkerA(void* /*argument*/) noexcept {
  for (uint32_t index = 0; index < 3U; ++index) {
    (void)PrintLn("sched: worker a");
    (void)sched::ThreadSleep(3);
  }
  ++g_workers_done;
}

void WorkerB(void* /*argument*/) noexcept {
  for (uint32_t index = 0; index < 3U; ++index) {
    (void)PrintLn("sched: worker b");
    (void)sched::ThreadSleep(3);
  }
  ++g_workers_done;
}

void UserThreadEntry(void* /*argument*/) noexcept {
  arch::aarch64::EnterUserMode(kUserCodeVa, kUserStackVa + kUserStackSize);
}

Status LoadUserProgram() noexcept {
  const uint64_t code_size = (user::kProgramSize + 4095U) & ~4095U;
  Status status = mem::VmmMapUser(kUserCodeVa, code_size, true);
  if (IsErr(status)) {
    return status;
  }
  status = mem::VmmCopyOut(kUserCodeVa, user::kProgram, user::kProgramSize);
  if (IsErr(status)) {
    return status;
  }
  status = mem::VmmMapUser(kUserStackVa, kUserStackSize, false);
  if (IsErr(status)) {
    return status;
  }
  return Status::kOk;
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

Status BlockReadAdapter(void* context, uint64_t sector,
                        std::span<uint8_t> buffer) noexcept {
  if (context == nullptr) {
    return Status::kInvalidArg;
  }
  return static_cast<drivers::virtio::Blk*>(context)->read(sector, buffer);
}

void InitFilesystem() noexcept {
  if (!g_block_device.is_ready()) {
    Log(LogLevel::kWarn, "fs: no block device\n");
    return;
  }

  Status status = fs::FsInit();
  if (IsErr(status)) {
    Log(LogLevel::kError, "fs: init failed\n");
    return;
  }

  Result<fs::Node*> dev_root = fs::devfs::Create();
  if (dev_root.is_err()) {
    Log(LogLevel::kError, "fs: devfs create failed\n");
    return;
  }
  status = fs::FsMount("/dev", dev_root.value());
  if (IsErr(status)) {
    Log(LogLevel::kError, "fs: devfs mount failed\n");
    return;
  }

  const fs::BlockDevice disk{&BlockReadAdapter, &g_block_device,
                             g_block_device.capacity_sectors(), 0};
  const Result<fs::Partition> partition = fs::FindEspPartition(disk);
  if (partition.is_err()) {
    Log(LogLevel::kError, "fs: no esp partition found\n");
    return;
  }
  const fs::BlockDevice device{&BlockReadAdapter, &g_block_device, partition.value().sector_count,
                               partition.value().start_sector};
  fs::Node* fat_root = nullptr;
  status = fs::fat32::Mount(device, &fat_root);
  if (IsErr(status)) {
    Log(LogLevel::kError, "fs: fat32 mount failed\n");
    return;
  }
  status = fs::FsMount("/", fat_root);
  if (IsErr(status)) {
    Log(LogLevel::kError, "fs: root mount failed\n");
    return;
  }
  Log(LogLevel::kInfo, "fs: mounted devfs and fat32\n");

  Result<uint32_t> console = fs::FsOpen("/dev/console");
  if (console.is_ok()) {
    constexpr char kMessage[] = "fs: devfs console ok\n";
    uint64_t written = 0;
    (void)fs::FsWrite(console.value(),
                      std::span<const uint8_t>(
                        reinterpret_cast<const uint8_t*>(kMessage), sizeof(kMessage) - 1U),
                      written);
    (void)fs::FsClose(console.value());
  } else {
    Log(LogLevel::kError, "fs: open dev console failed\n");
  }

  Result<uint32_t> program = fs::FsOpen("/EFI/BOOT/BOOTAA64.EFI");
  if (program.is_err()) {
    Log(LogLevel::kError, "fs: open efi image failed\n");
    return;
  }

  uint8_t header[2] = {0, 0};
  uint64_t read = 0;
  status = fs::FsRead(program.value(), std::span<uint8_t>(header, sizeof(header)), read);
  if (IsErr(status) || read != sizeof(header)) {
    Log(LogLevel::kError, "fs: read efi header failed\n");
    (void)fs::FsClose(program.value());
    return;
  }
  if (header[0] != 'M' || header[1] != 'Z') {
    Log(LogLevel::kError, "fs: efi header mismatch\n");
    (void)fs::FsClose(program.value());
    return;
  }

  const Result<fs::Stat> stat = fs::FsStat("/EFI/BOOT/BOOTAA64.EFI");
  if (stat.is_ok()) {
    LogDec(LogLevel::kInfo, "fs: efi image bytes", stat.value().size);
  }
  (void)fs::FsClose(program.value());
  Log(LogLevel::kInfo, "fs: fat32 file read ok\n");

  for (uint32_t index = 0; index < 8U; ++index) {
    fs::DirEntry entry{};
    bool found = false;
    status = fs::FsList("/", index, entry, found);
    if (IsErr(status) || !found) {
      break;
    }
    (void)PrintLn(entry.name);
  }
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
  InitFilesystem();

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
  const uint64_t timeout = arch::aarch64::ReadCntfrqEl0() * kTimeoutSeconds;
  while (time::TimerTicks() < kTargetTicks) {
    if ((arch::aarch64::ReadCntpctEl0() - start) > timeout) {
      break;
    }
  }
  arch::aarch64::DisableIrq();

  const uint64_t ticks = time::TimerTicks();
  LogDec(LogLevel::kInfo, "timer: ticks", ticks);
  LogDec(LogLevel::kInfo, "timer: work runs", g_work_runs);
  if (ticks == 0) {
    Log(LogLevel::kError, "timer: no interrupts received\n");
    Panic("timer: irq not working");
  }
  if (ticks < kTargetTicks) {
    Log(LogLevel::kWarn, "timer: few ticks under host load\n");
  } else {
    Log(LogLevel::kInfo, "timer: periodic tick ok\n");
  }

  status = sched::SchedulerInit();
  if (IsErr(status)) {
    LogHex(LogLevel::kError, "sched: init status", static_cast<uint64_t>(status));
    Panic("sched: init failed");
  }
  const Result<sched::Thread*> worker_a = sched::ThreadCreate("worker-a", &WorkerA, nullptr);
  if (worker_a.is_err()) {
    Panic("sched: create worker a failed");
  }
  const Result<sched::Thread*> worker_b = sched::ThreadCreate("worker-b", &WorkerB, nullptr);
  if (worker_b.is_err()) {
    Panic("sched: create worker b failed");
  }
  LogDec(LogLevel::kInfo, "sched: threads", sched::ThreadCount());

  arch::aarch64::DisableIrq();
  sched::SchedulerStart();
  arch::aarch64::EnableIrq();

  uint32_t waits = 0;
  while (g_workers_done < 2U && waits < 2000U) {
    (void)sched::ThreadSleep(1);
    ++waits;
  }
  LogDec(LogLevel::kInfo, "sched: workers done", g_workers_done);
  if (g_workers_done < 2U) {
    Panic("sched: workers did not finish");
  }
  Log(LogLevel::kInfo, "sched: round robin ok\n");

  status = LoadUserProgram();
  if (IsErr(status)) {
    Panic("user: load failed");
  }
  syscall::SyscallInit();
  const Result<sched::Thread*> user_thread =
    sched::ThreadCreate("user", &UserThreadEntry, nullptr);
  if (user_thread.is_err()) {
    Panic("user: thread create failed");
  }
  LogDec(LogLevel::kInfo, "user: program bytes", user::kProgramSize);

  arch::aarch64::DisableIrq();
  sched::SchedulerStart();
  arch::aarch64::EnableIrq();

  if (!syscall::UserExited()) {
    Panic("user: exit not reached");
  }
  Log(LogLevel::kInfo, "user: exit ok\n");

  (void)PrintLn("kernel: halt");
  arch::aarch64::HaltForever();
}

}  // namespace og
