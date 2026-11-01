#include "boot.h"

#include "memory.h"
#include "og/core/log.h"
#include "og/core/status.h"
#include "og/drivers/device.h"
#include "og/drivers/virtio_blk.h"
#include "og/fs/block_device.h"
#include "og/fs/devfs.h"
#include "og/fs/fat32.h"
#include "og/fs/partition.h"
#include "og/fs/procfs.h"
#include "og/fs/ramfs.h"
#include "og/fs/vfs.h"
#include "og/io/print.h"
#include "og/mem/heap.h"
#include "og/sched/sched.h"

namespace og::boot {

namespace {

fs::Node* g_ram_root = nullptr;
fs::FileTable g_boot_files;

Status BlockReadAdapter(void* context, uint64_t sector,
                        std::span<uint8_t> buffer) noexcept {
  if (context == nullptr) {
    return Status::kInvalidArg;
  }
  return static_cast<drivers::virtio::Blk*>(context)->read(sector, buffer);
}

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

  const Result<fs::Stat> stat = fs::FsStat("/GEMABOX.ELF");
  if (stat.is_err()) {
    Log(LogLevel::kWarn, "fs: GEMABOX.ELF missing\n");
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
  const Result<uint32_t> file = fs::FsOpen(g_boot_files, "/GEMABOX.ELF");
  if (file.is_err()) {
    (void)mem::HeapFree(buffer.value());
    return file.status();
  }
  uint64_t read = 0;
  status = fs::FsRead(
    g_boot_files, file.value(),
    std::span<uint8_t>(static_cast<uint8_t*>(buffer.value()), size), read);
  (void)fs::FsClose(g_boot_files, file.value());
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
  const Result<fs::Node*> writer = fs::ramfs::CreateFile(g_ram_root, "writer", nullptr, 0);
  if (writer.is_err()) {
    return writer.status();
  }

  const Result<uint32_t> hello_file = fs::FsOpen(g_boot_files, "/ram/hello");
  if (hello_file.is_err()) {
    return hello_file.status();
  }
  constexpr char kHello[] = "ramfs write ok\n";
  uint64_t written = 0;
  status = fs::FsWrite(
    g_boot_files, hello_file.value(),
    std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(kHello), sizeof(kHello) - 1U),
    written);
  if (IsErr(status) || written != (sizeof(kHello) - 1U)) {
    (void)fs::FsClose(g_boot_files, hello_file.value());
    return Status::kIoError;
  }
  status = fs::FsSeek(g_boot_files, hello_file.value(), 0);
  if (IsErr(status)) {
    (void)fs::FsClose(g_boot_files, hello_file.value());
    return status;
  }
  char readback[32];
  uint64_t readback_size = 0;
  status = fs::FsRead(g_boot_files, hello_file.value(),
                      std::span<uint8_t>(reinterpret_cast<uint8_t*>(readback),
                                         sizeof(kHello) - 1U),
                      readback_size);
  (void)fs::FsClose(g_boot_files, hello_file.value());
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

  const Result<uint32_t> uptime = fs::FsOpen(g_boot_files, "/proc/uptime");
  if (uptime.is_err()) {
    return uptime.status();
  }
  char text[32];
  uint64_t read = 0;
  const Status read_status = fs::FsRead(
    g_boot_files, uptime.value(),
    std::span<uint8_t>(reinterpret_cast<uint8_t*>(text), sizeof(text) - 1U), read);
  (void)fs::FsClose(g_boot_files, uptime.value());
  if (IsErr(read_status)) {
    return read_status;
  }
  text[read] = '\0';
  (void)PrintText("proc: uptime ");
  (void)PrintText(text);
  return Status::kOk;
}

}  // namespace

void InitFilesystem() noexcept {
  const Result<drivers::Device> found = drivers::FindDeviceByName("virtio-blk0");
  if (found.is_err() || found.value().driver_data == nullptr) {
    Log(LogLevel::kWarn, "fs: no block device\n");
    return;
  }
  auto* block = static_cast<drivers::virtio::Blk*>(found.value().driver_data);
  if (!block->is_ready()) {
    Log(LogLevel::kWarn, "fs: block device not ready\n");
    return;
  }

  Status status = fs::FsInit();
  if (IsErr(status)) {
    Log(LogLevel::kError, "fs: init failed\n");
    return;
  }
  status = fs::FileTableInit(g_boot_files);
  if (IsErr(status)) {
    Log(LogLevel::kError, "fs: boot file table failed\n");
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

  const fs::BlockDevice disk{&BlockReadAdapter, block, block->capacity_sectors(), 0};
  const Result<fs::Partition> partition = fs::FindEspPartition(disk);
  if (partition.is_err()) {
    Log(LogLevel::kError, "fs: no esp partition found\n");
    return;
  }
  const fs::BlockDevice device{&BlockReadAdapter, block, partition.value().sector_count,
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

  Result<uint32_t> console = fs::FsOpen(g_boot_files, "/dev/console");
  if (console.is_ok()) {
    constexpr char kMessage[] = "fs: devfs console ok\n";
    uint64_t written = 0;
    (void)fs::FsWrite(g_boot_files, console.value(),
                      std::span<const uint8_t>(
                        reinterpret_cast<const uint8_t*>(kMessage), sizeof(kMessage) - 1U),
                      written);
    (void)fs::FsClose(g_boot_files, console.value());
  } else {
    Log(LogLevel::kError, "fs: open dev console failed\n");
  }

  Result<uint32_t> program = fs::FsOpen(g_boot_files, "/EFI/BOOT/BOOTAA64.EFI");
  if (program.is_err()) {
    Log(LogLevel::kError, "fs: open efi image failed\n");
    return;
  }

  uint8_t header[2] = {0, 0};
  uint64_t read = 0;
  status = fs::FsRead(
    g_boot_files, program.value(),
    std::span<uint8_t>(header, sizeof(header)), read);
  if (IsErr(status) || read != sizeof(header)) {
    Log(LogLevel::kError, "fs: read efi header failed\n");
    (void)fs::FsClose(g_boot_files, program.value());
    return;
  }
  if (header[0] != 'M' || header[1] != 'Z') {
    Log(LogLevel::kError, "fs: efi header mismatch\n");
    (void)fs::FsClose(g_boot_files, program.value());
    return;
  }

  const Result<fs::Stat> stat = fs::FsStat("/EFI/BOOT/BOOTAA64.EFI");
  if (stat.is_ok()) {
    LogDec(LogLevel::kInfo, "fs: efi image bytes", stat.value().size);
  }
  (void)fs::FsClose(g_boot_files, program.value());
  Log(LogLevel::kInfo, "fs: fat32 file read ok\n");

  for (uint32_t index = 0; index < 8U; ++index) {
    fs::DirEntry entry{};
    bool found_entry = false;
    status = fs::FsList("/", index, entry, found_entry);
    if (IsErr(status) || !found_entry) {
      break;
    }
    (void)PrintLn(entry.name);
  }
}

}  // namespace og::boot
