#include "og/arch/cpu.h"
#include "og/arch/registers.h"
#include "og/arch/user.h"

#include "memory.h"
#include "og/core/bootinfo.h"
#include "og/core/log.h"
#include "og/core/panic.h"
#include "og/core/status.h"
#include "og/dma/dma.h"
#include "og/dma/dma_selftest.h"
#include "og/drivers/device.h"
#include "og/drivers/gic.h"
#include "og/drivers/pci.h"
#include "og/drivers/rtc.h"
#include "og/drivers/uart.h"
#include "og/drivers/virtio_blk.h"
#include "og/drivers/virtio_gpu.h"
#include "og/events/event_selftest.h"
#include "og/events/irq.h"
#include "og/events/trap.h"
#include "og/events/workqueue.h"
#include "og/fs/block_device.h"
#include "og/fs/devfs.h"
#include "og/fs/fat32.h"
#include "og/fs/partition.h"
#include "og/fs/procfs.h"
#include "og/fs/ramfs.h"
#include "og/fs/vfs.h"
#include "og/io/console.h"
#include "og/io/print.h"
#include "og/mem/heap.h"
#include "og/mem/mem_selftest.h"
#include "og/mem/pmm.h"
#include "og/mem/vmm.h"
#include "og/platform/memory_map.h"
#include "og/platform/platform.h"
#include "og/platform/psci.h"
#include "og/platform/smp.h"
#include "og/sched/sched.h"
#include "og/sync/sync.h"
#include "og/syscall/syscall.h"
#include "og/time/timer.h"

#include "og/events/workqueue.h"

extern "C" uintptr_t __stack_chk_guard;
extern "C" uintptr_t __security_cookie;

namespace og {

namespace {

// The host can starve the vCPU, so prove interrupts arrive without
// asserting a wall clock rate. Only zero ticks is treated as a failure.
constexpr uint64_t kTargetTicks = 10;
constexpr uint64_t kTimeoutSeconds = 20;
constexpr uint64_t kSectorSize = 512;
constexpr uint64_t kUserCodeVa = mem::kUserSpaceBase;
constexpr uint64_t kUserStackVa = mem::kUserSpaceBase + 0x100000ULL;
constexpr uint64_t kUserStackSize = 16384;
uint64_t g_work_runs = 0;
uint64_t g_workers_done = 0;
uint64_t g_worker_cpu_mask = 0;
uint64_t g_ipi_count = 0;
uint64_t g_mutex_counter = 0;
uint64_t g_mutex_done = 0;
uint64_t g_semaphore_seen = 0;
uint64_t g_once_runs = 0;
uint64_t g_timeout_fired = 0;
uint64_t g_uart_rx_bytes = 0;
sync::Mutex g_test_mutex;
sync::Semaphore g_test_semaphore;
drivers::virtio::Blk g_block_device;
drivers::virtio::Gpu g_gpu;
drivers::rtc::Pl031 g_rtc;
io::Tee g_console_tee;
mem::AddressSpace g_user_space;

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

void MutexWorker(void* /*argument*/) noexcept {
  for (uint32_t index = 0; index < 50U; ++index) {
    if (IsErr(g_test_mutex.lock())) {
      break;
    }
    const uint64_t value = g_mutex_counter;
    g_mutex_counter = value + 1U;
    g_test_mutex.unlock();
    (void)sched::ThreadSleep(1);
  }
  (void)__atomic_add_fetch(&g_mutex_done, 1U, __ATOMIC_RELAXED);
}

void SemaphoreWorker(void* /*argument*/) noexcept {
  (void)g_test_semaphore.wait();
  (void)__atomic_add_fetch(&g_semaphore_seen, 1U, __ATOMIC_RELAXED);
}

void OnceHandler(void* /*argument*/) noexcept {
  (void)__atomic_add_fetch(&g_once_runs, 1U, __ATOMIC_RELAXED);
}

void TimeoutHandler(void* /*argument*/) noexcept {
  (void)__atomic_add_fetch(&g_timeout_fired, 1U, __ATOMIC_RELAXED);
}

void UartRxWorker(void* /*argument*/) noexcept {
  char buffer[4];
  for (uint32_t waits = 0; waits < 5000U; ++waits) {
    drivers::uart::Pl011* uart = drivers::uart::Active();
    if (uart != nullptr) {
      const uint32_t received = uart->read(std::span<char>(buffer, sizeof(buffer)));
      if (received == sizeof(buffer)) {
        __atomic_store_n(&g_uart_rx_bytes, 4U, __ATOMIC_RELEASE);
        return;
      }
    }
    (void)sched::ThreadSleep(1);
  }
}

Status IpiHandler(void* /*context*/, uint32_t /*irq*/) noexcept {
  __atomic_add_fetch(&g_ipi_count, 1U, __ATOMIC_RELAXED);
  return Status::kOk;
}

void CountWork(void* /*context*/) noexcept {
  ++g_work_runs;
}

void WorkerA(void* /*argument*/) noexcept {
  for (uint32_t index = 0; index < 3U; ++index) {
    (void)__atomic_or_fetch(&g_worker_cpu_mask,
                            1ULL << smp::CurrentIndex(), __ATOMIC_RELAXED);
    (void)PrintLn("sched: worker a");
    (void)sched::ThreadSleep(3);
  }
  ++g_workers_done;
}

void WorkerB(void* /*argument*/) noexcept {
  for (uint32_t index = 0; index < 3U; ++index) {
    (void)__atomic_or_fetch(&g_worker_cpu_mask,
                            1ULL << smp::CurrentIndex(), __ATOMIC_RELAXED);
    (void)PrintLn("sched: worker b");
    (void)sched::ThreadSleep(3);
  }
  ++g_workers_done;
}

constexpr uint64_t kAddressSpaceTestVa = mem::kUserSpaceBase + 0x3000000ULL;

struct AddressSpaceTestState {
  uint64_t expected;
  uint64_t observed;
  uint32_t done;
};

mem::AddressSpace g_as_a;
mem::AddressSpace g_as_b;
AddressSpaceTestState g_as_state_a{};
AddressSpaceTestState g_as_state_b{};

void AddressSpaceWorker(void* argument) noexcept {
  auto* state = static_cast<AddressSpaceTestState*>(argument);
  uint64_t observed = 0;
  mem::Copy(&observed, reinterpret_cast<const void*>(kAddressSpaceTestVa),
            sizeof(observed));
  state->observed = observed;
  __atomic_store_n(&state->done, 1U, __ATOMIC_RELEASE);
}

void RunAddressSpaceTest() noexcept {
  Status status = mem::AddressSpaceInit(g_as_a);
  if (IsErr(status)) {
    Panic("mem: address space a init failed");
  }
  status = mem::AddressSpaceInit(g_as_b);
  if (IsErr(status)) {
    Panic("mem: address space b init failed");
  }

  constexpr uint64_t kValueA = 0x1122334455667788ULL;
  constexpr uint64_t kValueB = 0x99AABBCCDDEEFF00ULL;
  status = mem::VmmMapUser(g_as_a, kAddressSpaceTestVa, 4096, false);
  if (IsErr(status)) {
    Panic("mem: address space a map failed");
  }
  status = mem::VmmMapUser(g_as_b, kAddressSpaceTestVa, 4096, false);
  if (IsErr(status)) {
    Panic("mem: address space b map failed");
  }
  status = mem::VmmCopyOut(g_as_a, kAddressSpaceTestVa, &kValueA, sizeof(kValueA));
  if (IsErr(status)) {
    Panic("mem: address space a copy failed");
  }
  status = mem::VmmCopyOut(g_as_b, kAddressSpaceTestVa, &kValueB, sizeof(kValueB));
  if (IsErr(status)) {
    Panic("mem: address space b copy failed");
  }

  g_as_state_a.expected = kValueA;
  g_as_state_a.done = 0;
  g_as_state_b.expected = kValueB;
  g_as_state_b.done = 0;

  const Result<sched::Thread*> thread_a =
    sched::ThreadCreateIn("as-a", &AddressSpaceWorker, &g_as_state_a, &g_as_a);
  const Result<sched::Thread*> thread_b =
    sched::ThreadCreateIn("as-b", &AddressSpaceWorker, &g_as_state_b, &g_as_b);
  if (thread_a.is_err() || thread_b.is_err()) {
    Panic("mem: address space thread create failed");
  }

  uint32_t waits = 0;
  while ((__atomic_load_n(&g_as_state_a.done, __ATOMIC_ACQUIRE) == 0U ||
          __atomic_load_n(&g_as_state_b.done, __ATOMIC_ACQUIRE) == 0U) &&
         waits < 1000U) {
    (void)sched::ThreadSleep(1);
    ++waits;
  }
  if (__atomic_load_n(&g_as_state_a.done, __ATOMIC_ACQUIRE) == 0U ||
      __atomic_load_n(&g_as_state_b.done, __ATOMIC_ACQUIRE) == 0U) {
    Panic("mem: address space workers did not finish");
  }
  if (g_as_state_a.observed != kValueA || g_as_state_b.observed != kValueB) {
    Panic("mem: address spaces overlap");
  }

  waits = 0;
  while ((!sched::ThreadFinished(thread_a.value()) ||
          !sched::ThreadFinished(thread_b.value())) &&
         waits < 1000U) {
    (void)sched::ThreadSleep(1);
    ++waits;
  }
  (void)mem::AddressSpaceDestroy(g_as_a);
  (void)mem::AddressSpaceDestroy(g_as_b);
  Log(LogLevel::kInfo, "mem: address space switch ok\n");
}

void UserThreadEntry(void* /*argument*/) noexcept {
  arch::aarch64::EnterUserMode(kUserCodeVa, kUserStackVa + kUserStackSize);
}

Status LoadUserProgram() noexcept {
  const Result<fs::Stat> stat = fs::FsStat("/ram/gemabox");
  if (stat.is_err()) {
    return stat.status();
  }
  const uint64_t size = stat.value().size;
  if (size == 0 || size > 65536U) {
    return Status::kInvalidArg;
  }

  const Result<void*> buffer = mem::HeapAlloc(size);
  if (buffer.is_err()) {
    return buffer.status();
  }
  const Result<uint32_t> file = fs::FsOpen("/ram/gemabox");
  if (file.is_err()) {
    (void)mem::HeapFree(buffer.value());
    return file.status();
  }
  uint64_t read = 0;
  Status status = fs::FsRead(
    file.value(),
    std::span<uint8_t>(static_cast<uint8_t*>(buffer.value()), size), read);
  (void)fs::FsClose(file.value());
  if (IsErr(status) || read != size) {
    (void)mem::HeapFree(buffer.value());
    return Status::kIoError;
  }

  const uint64_t code_size = (size + 4095U) & ~4095U;
  if (g_user_space.root == 0) {
    status = mem::AddressSpaceInit(g_user_space);
    if (IsErr(status)) {
      (void)mem::HeapFree(buffer.value());
      return status;
    }
  }
  status = mem::VmmMapUser(g_user_space, kUserCodeVa, code_size, true);
  if (IsErr(status)) {
    (void)mem::HeapFree(buffer.value());
    return status;
  }
  status = mem::VmmCopyOut(g_user_space, kUserCodeVa, buffer.value(), size);
  (void)mem::HeapFree(buffer.value());
  if (IsErr(status)) {
    return status;
  }
  status = mem::VmmMapUser(g_user_space, kUserStackVa, kUserStackSize, false);
  if (IsErr(status)) {
    return status;
  }
  LogDec(LogLevel::kInfo, "user: program bytes", size);
  return Status::kOk;
}

fs::Node* g_ram_root = nullptr;

Status PopulateRamfs() noexcept {
  Result<fs::Node*> ram = fs::ramfs::Create();
  if (ram.is_err()) {
    return ram.status();
  }
  g_ram_root = ram.value();
  Status status = fs::FsMount("/ram", g_ram_root);
  if (IsErr(status)) {
    return status;
  }

  const Result<fs::Stat> stat = fs::FsStat("/GEMABOX.BIN");
  if (stat.is_err()) {
    Log(LogLevel::kWarn, "fs: GEMABOX.BIN missing\n");
    return stat.status();
  }
  const uint64_t size = stat.value().size;
  if (size == 0 || size > 65536U) {
    return Status::kInvalidArg;
  }

  const Result<void*> buffer = mem::HeapAlloc(size);
  if (buffer.is_err()) {
    return buffer.status();
  }
  const Result<uint32_t> file = fs::FsOpen("/GEMABOX.BIN");
  if (file.is_err()) {
    (void)mem::HeapFree(buffer.value());
    return file.status();
  }
  uint64_t read = 0;
  status = fs::FsRead(
    file.value(),
    std::span<uint8_t>(static_cast<uint8_t*>(buffer.value()), size), read);
  (void)fs::FsClose(file.value());
  if (IsErr(status) || read != size) {
    (void)mem::HeapFree(buffer.value());
    return Status::kIoError;
  }

  const Result<fs::Node*> init =
    fs::ramfs::CreateFile(g_ram_root, "gemabox", buffer.value(), size);
  (void)mem::HeapFree(buffer.value());
  if (init.is_err()) {
    return init.status();
  }
  LogDec(LogLevel::kInfo, "fs: ramfs gemabox bytes", size);

  const Result<fs::Node*> hello = fs::ramfs::CreateFile(g_ram_root, "hello", nullptr, 0);
  if (hello.is_err()) {
    return hello.status();
  }

  const Result<uint32_t> hello_file = fs::FsOpen("/ram/hello");
  if (hello_file.is_err()) {
    return hello_file.status();
  }
  constexpr char kHello[] = "ramfs write ok\n";
  uint64_t written = 0;
  status = fs::FsWrite(
    hello_file.value(),
    std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(kHello), sizeof(kHello) - 1U),
    written);
  if (IsErr(status) || written != (sizeof(kHello) - 1U)) {
    (void)fs::FsClose(hello_file.value());
    return Status::kIoError;
  }
  status = fs::FsSeek(hello_file.value(), 0);
  if (IsErr(status)) {
    (void)fs::FsClose(hello_file.value());
    return status;
  }
  char readback[32];
  uint64_t readback_size = 0;
  status = fs::FsRead(hello_file.value(),
                      std::span<uint8_t>(reinterpret_cast<uint8_t*>(readback),
                                         sizeof(kHello) - 1U),
                      readback_size);
  (void)fs::FsClose(hello_file.value());
  if (IsErr(status) || !mem::Equal(readback, kHello, sizeof(kHello) - 1U)) {
    return Status::kIoError;
  }
  Log(LogLevel::kInfo, "fs: ramfs write ok\n");
  return Status::kOk;
}

Status MountProcfs() noexcept {
  Result<fs::Node*> proc = fs::procfs::Create(&sched::ThreadCount);
  if (proc.is_err()) {
    return proc.status();
  }
  const Status status = fs::FsMount("/proc", proc.value());
  if (IsErr(status)) {
    return status;
  }

  const Result<uint32_t> uptime = fs::FsOpen("/proc/uptime");
  if (uptime.is_err()) {
    return uptime.status();
  }
  char text[32];
  uint64_t read = 0;
  const Status read_status =
    fs::FsRead(uptime.value(), std::span<uint8_t>(reinterpret_cast<uint8_t*>(text), sizeof(text) - 1U), read);
  (void)fs::FsClose(uptime.value());
  if (IsErr(read_status)) {
    return read_status;
  }
  text[read] = '\0';
  (void)PrintText("proc: uptime ");
  (void)PrintText(text);
  return Status::kOk;
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

  const Status ram_status = PopulateRamfs();
  if (IsErr(ram_status)) {
    Log(LogLevel::kError, "fs: ramfs setup failed\n");
  }
  const Status proc_status = MountProcfs();
  if (IsErr(proc_status)) {
    Log(LogLevel::kWarn, "fs: procfs setup failed\n");
  }

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

  const Status platform_status = platform::PlatformInit(*info);
  if (IsErr(platform_status)) {
    Log(LogLevel::kWarn, "platform: dtb unavailable\n");
  } else {
    Log(LogLevel::kInfo, "platform: dtb parsed\n");
  }
  LogDec(LogLevel::kInfo, "platform: cpus", platform::Current().cpu_count);
  LogDec(LogLevel::kInfo, "platform: psci conduit", platform::Current().psci_conduit);

  // Install vectors first so any fault after this point panics with context.
  events::InstallVectorTable();

  __stack_chk_guard = arch::aarch64::ReadCntpctEl0() ^ 0x9E3779B97F4A7C15ULL;
  __security_cookie = __stack_chk_guard;

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

  if (info->text_size != 0) {
    const Result<mem::PageAttributes> text_attributes =
      mem::VmmQueryAttributes(info->text_base);
    if (text_attributes.is_err()) {
      Panic("hardening: text attributes missing");
    }
    if (!text_attributes.value().executable_el1 || text_attributes.value().writable) {
      Panic("hardening: text is not w^x");
    }
  }

  status = mem::HeapInit();
  if (IsErr(status)) {
    Panic("mem: heap init failed");
  }

  const Result<void*> probe = mem::HeapAlloc(16);
  if (probe.is_err()) {
    Panic("hardening: heap probe failed");
  }
  const Result<mem::PageAttributes> heap_attributes =
    mem::VmmQueryAttributes(reinterpret_cast<uint64_t>(probe.value()));
  if (heap_attributes.is_err()) {
    Panic("hardening: heap attributes missing");
  }
  if (heap_attributes.value().executable_el1 || !heap_attributes.value().writable) {
    Panic("hardening: heap is not n^x");
  }
  (void)mem::HeapFree(probe.value());
  Log(LogLevel::kInfo, "hardening: wx attributes ok\n");

  status = mem::RunSelfTest();
  if (IsErr(status)) {
    Panic("mem: self test failed");
  }

  status = dma::RunDmaSelfTest();
  if (IsErr(status)) {
    Panic("dma: self test failed");
  }

  InitBlockDevice();
  InitGpu();
  InitRtc();
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
  status = gic.init(platform::Current().gic_distributor.base,
                    platform::Current().gic_redistributor.base);
  if (IsErr(status)) {
    Panic("gic: init failed");
  }
  events::SetInterruptController(drivers::irq::MakeController(gic));
  Log(LogLevel::kInfo, "gic: ready\n");

  drivers::uart::Pl011* uart = drivers::uart::Active();
  if (uart != nullptr) {
    const Status rx_status =
      uart->enable_receive(drivers::uart::kUartIrq, platform::Current().uart_clock, 115200);
    if (IsErr(rx_status)) {
      Log(LogLevel::kWarn, "uart: rx init failed\n");
    } else {
      const Status irq_status = gic.enable_irq(drivers::uart::kUartIrq);
      if (IsErr(irq_status)) {
        Log(LogLevel::kWarn, "uart: rx irq enable failed\n");
      }
    }
  }

  status = time::TimerInit();
  if (IsErr(status)) {
    Panic("timer: init failed");
  }
  status = gic.enable_irq(time::kTimerIrq);
  if (IsErr(status)) {
    Panic("timer: irq enable failed");
  }
  LogDec(LogLevel::kInfo, "timer: frequency", time::TimerFrequency());

  status = time::TimerStartPeriodic(sched::kTickHz);
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

  status = psci::PsciInit(platform::Current().psci_conduit);
  if (IsErr(status)) {
    Log(LogLevel::kWarn, "psci: not available\n");
  } else {
    LogDec(LogLevel::kInfo, "psci: version", psci::Version());
    status = smp::SmpInit();
    if (IsErr(status)) {
      Log(LogLevel::kError, "smp: init failed\n");
    } else {
      status = events::RegisterIrq(smp::kIpiIrq, &IpiHandler, nullptr);
      if (IsErr(status)) {
        Log(LogLevel::kError, "smp: ipi register failed\n");
      }
      status = smp::StartSecondaries();
      if (IsErr(status)) {
        LogHex(LogLevel::kError, "smp: start status", static_cast<uint64_t>(status));
      } else {
        LogDec(LogLevel::kInfo, "smp: cpus online", smp::OnlineCount());
        drivers::irq::Gicv3::SendSgi(smp::kIpiIrq, 1);

        const uint64_t ipi_start = arch::aarch64::ReadCntpctEl0();
        const uint64_t ipi_timeout = arch::aarch64::ReadCntfrqEl0() * 2ULL;
        while (__atomic_load_n(&g_ipi_count, __ATOMIC_ACQUIRE) == 0U) {
          if ((arch::aarch64::ReadCntpctEl0() - ipi_start) > ipi_timeout) {
            break;
          }
        }
        const uint64_t ipis = __atomic_load_n(&g_ipi_count, __ATOMIC_ACQUIRE);
        LogDec(LogLevel::kInfo, "smp: ipi count", ipis);
        if (ipis == 0U) {
          Log(LogLevel::kError, "smp: ipi not received\n");
        }
      }
    }
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
  const uint64_t worker_cpus =
    __atomic_load_n(&g_worker_cpu_mask, __ATOMIC_ACQUIRE);
  LogDec(LogLevel::kInfo, "smp: worker cpus", worker_cpus);
  const uint32_t online = smp::OnlineCount();
  if (online < 64U) {
    const uint64_t expected = (1ULL << online) - 1ULL;
    if ((worker_cpus & expected) != expected) {
      Panic("sched: workers stayed on one cpu");
    }
  }
  Log(LogLevel::kInfo, "sched: round robin ok\n");

  RunAddressSpaceTest();

  g_test_mutex.set_name("self-test");
  g_test_semaphore.init(0);
  if (sched::ThreadCreate("mutex-a", &MutexWorker, nullptr).is_err() ||
      sched::ThreadCreate("mutex-b", &MutexWorker, nullptr).is_err() ||
      sched::ThreadCreate("semaphore", &SemaphoreWorker, nullptr).is_err()) {
    Panic("sync: thread create failed");
  }
  g_test_semaphore.signal();

  uint32_t sync_waits = 0;
  while ((__atomic_load_n(&g_mutex_done, __ATOMIC_ACQUIRE) < 2U ||
          __atomic_load_n(&g_semaphore_seen, __ATOMIC_ACQUIRE) < 1U) &&
         sync_waits < 5000U) {
    (void)sched::ThreadSleep(1);
    ++sync_waits;
  }
  if (g_mutex_counter != 100U) {
    Panic("sync: mutex lost updates");
  }
  if (g_semaphore_seen != 1U) {
    Panic("sync: semaphore failed");
  }

  sync::RefCount refs;
  refs.reset(1);
  if (refs.add(2) != 3U) {
    Panic("sync: refcount add failed");
  }
  if (refs.release() || refs.release()) {
    Panic("sync: refcount released early");
  }
  if (!refs.release()) {
    Panic("sync: refcount did not reach zero");
  }

  sync::OnceFlag once{};
  sync::CallOnce(once, &OnceHandler, nullptr);
  sync::CallOnce(once, &OnceHandler, nullptr);
  if (g_once_runs != 1U) {
    Panic("sync: once ran twice");
  }
  Log(LogLevel::kInfo, "sync: mutex, semaphore, refcount, once ok\n");

  const Result<time::Timeout*> scheduled_timeout =
    time::ScheduleAfter(5, &TimeoutHandler, nullptr);
  if (scheduled_timeout.is_err()) {
    Panic("time: schedule failed");
  }
  uint32_t timeout_waits = 0;
  while (__atomic_load_n(&g_timeout_fired, __ATOMIC_ACQUIRE) == 0U && timeout_waits < 500U) {
    (void)sched::ThreadSleep(1);
    ++timeout_waits;
  }
  if (g_timeout_fired != 1U) {
    Panic("time: timeout did not fire");
  }
  Log(LogLevel::kInfo, "time: timeout ok\n");

  (void)PrintLn("uart: ready");
  if (sched::ThreadCreate("uart-rx", &UartRxWorker, nullptr).is_err()) {
    Panic("uart: rx thread create failed");
  }
  uint32_t uart_waits = 0;
  while (__atomic_load_n(&g_uart_rx_bytes, __ATOMIC_ACQUIRE) < 4U && uart_waits < 5000U) {
    (void)sched::ThreadSleep(1);
    ++uart_waits;
  }
  if (__atomic_load_n(&g_uart_rx_bytes, __ATOMIC_ACQUIRE) == 4U) {
    Log(LogLevel::kInfo, "uart: rx ok\n");
  } else {
    Log(LogLevel::kWarn, "uart: rx timeout\n");
  }

  status = LoadUserProgram();
  if (IsErr(status)) {
    Panic("user: load failed");
  }
  syscall::SyscallInit();
  const Result<sched::Thread*> user_thread =
    sched::ThreadCreateIn("user", &UserThreadEntry, nullptr, &g_user_space);
  if (user_thread.is_err()) {
    Panic("user: thread create failed");
  }

  arch::aarch64::DisableIrq();
  sched::SchedulerStart();
  arch::aarch64::EnableIrq();

  uint32_t user_waits = 0;
  while (!syscall::UserExited() && user_waits < 5000U) {
    (void)sched::ThreadSleep(1);
    ++user_waits;
  }
  if (!syscall::UserExited()) {
    Panic("user: exit not reached");
  }
  Log(LogLevel::kInfo, "user: exit ok\n");
  uint32_t exit_waits = 0;
  while (!sched::ThreadFinished(user_thread.value()) && exit_waits < 1000U) {
    (void)sched::ThreadSleep(1);
    ++exit_waits;
  }
  (void)mem::AddressSpaceDestroy(g_user_space);

  (void)PrintLn("kernel: halt");
  arch::aarch64::HaltForever();
}

}  // namespace og
