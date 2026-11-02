#include "og/boot/init.h"

#include "format.h"
#include "gsl/layout.h"
#include "memory.h"
#include "og/core/log.h"
#include "og/core/status.h"
#include "og/drivers/device.h"
#include "og/drivers/virtio_blk.h"
#include "og/fs/block_device.h"
#include "og/fs/devfs.h"
#include "og/fs/fat32.h"
#include "og/fs/gslfs.h"
#include "og/fs/partition.h"
#include "og/fs/procfs.h"
#include "og/fs/ramfs.h"
#include "og/fs/vfs.h"
#include "og/io/print.h"
#include "og/mem/heap.h"
#include "og/process/process.h"
#include "og/sched/sched.h"

namespace og::boot {

namespace {

fs::FileTable g_boot_files;

// Test file under the fixed /tmp mount.
constexpr char kHelloPath[] = "/tmp/hello";

Status BlockReadAdapter(void* context, uint64_t sector,
                        std::span<uint8_t> buffer) noexcept {
  if (context == nullptr) {
    return Status::kInvalidArg;
  }
  return static_cast<drivers::virtio::Blk*>(context)->read(sector, buffer);
}

Status BlockWriteAdapter(void* context, uint64_t sector,
                         std::span<const uint8_t> buffer) noexcept {
  if (context == nullptr) {
    return Status::kInvalidArg;
  }
  return static_cast<drivers::virtio::Blk*>(context)->write(sector, buffer);
}

struct WritableTree {
  fs::Node* etc;
  fs::Node* users;
  fs::Node* var;
  fs::Node* tmp;
};

Status CreateDirChecked(fs::Node* parent, const char* name) noexcept {
  const Result<fs::Node*> dir = fs::ramfs::CreateDir(parent, name);
  return dir.is_err() ? dir.status() : Status::kOk;
}

Result<WritableTree> CreateWritableTree() noexcept {
  const Result<fs::Node*> ram = fs::ramfs::Create();
  if (ram.is_err()) {
    return Result<WritableTree>::Err(ram.status());
  }
  const Result<fs::Node*> etc = fs::ramfs::CreateDir(ram.value(), "etc");
  if (etc.is_err()) {
    return Result<WritableTree>::Err(etc.status());
  }
  const Result<fs::Node*> users = fs::ramfs::CreateDir(ram.value(), "users");
  if (users.is_err()) {
    return Result<WritableTree>::Err(users.status());
  }
  const Result<fs::Node*> var = fs::ramfs::CreateDir(ram.value(), "var");
  if (var.is_err()) {
    return Result<WritableTree>::Err(var.status());
  }
  const Result<fs::Node*> tmp = fs::ramfs::CreateDir(ram.value(), "tmp");
  if (tmp.is_err()) {
    return Result<WritableTree>::Err(tmp.status());
  }

  if (IsErr(CreateDirChecked(users.value(), "demo")) ||
      IsErr(CreateDirChecked(var.value(), "log")) ||
      IsErr(CreateDirChecked(var.value(), "run")) ||
      IsErr(CreateDirChecked(var.value(), "cache")) ||
      IsErr(CreateDirChecked(var.value(), "spool"))) {
    return Result<WritableTree>::Err(Status::kNoMem);
  }
  if (fs::ramfs::CreateFile(tmp.value(), "hello", nullptr, 0).is_err() ||
      fs::ramfs::CreateFile(tmp.value(), "writer", nullptr, 0).is_err()) {
    return Result<WritableTree>::Err(Status::kNoMem);
  }

  WritableTree tree{};
  tree.etc = etc.value();
  tree.users = users.value();
  tree.var = var.value();
  tree.tmp = tmp.value();
  return Result<WritableTree>::Ok(tree);
}

Status MountWritableTree(const WritableTree& tree) noexcept {
  Status status = fs::FsMount(gsl::layout::kEtc, tree.etc);
  if (IsErr(status)) {
    return status;
  }
  status = fs::FsMount(gsl::layout::kUsers, tree.users);
  if (IsErr(status)) {
    return status;
  }
  status = fs::FsMount(gsl::layout::kVar, tree.var);
  if (IsErr(status)) {
    return status;
  }
  return fs::FsMount(gsl::layout::kTmp, tree.tmp);
}

Status SeedEtcConfig(fs::Node* etc) noexcept {
  const Result<uint32_t> source =
    fs::FsOpen(g_boot_files, gsl::layout::kConfigGsl);
  if (source.is_err()) {
    Log(LogLevel::kWarn, "fs: gsl.conf default missing\n");
    return Status::kOk;
  }

  char content[256];
  uint64_t read = 0;
  Status status = fs::FsRead(
    g_boot_files, source.value(),
    std::span<uint8_t>(reinterpret_cast<uint8_t*>(content), sizeof(content)),
    read);
  (void)fs::FsClose(g_boot_files, source.value());
  if (IsErr(status)) {
    return status;
  }
  const Result<fs::Node*> copy =
    fs::ramfs::CreateFile(etc, "gsl.conf", content, read);
  if (copy.is_err()) {
    return copy.status();
  }
  Log(LogLevel::kInfo, "fs: etc defaults copied\n");
  return Status::kOk;
}

Status VerifyTmpWrite() noexcept {
  const Result<uint32_t> hello_file = fs::FsOpen(g_boot_files, kHelloPath, 1U);
  if (hello_file.is_err()) {
    return hello_file.status();
  }
  constexpr char kHello[] = "ramfs write ok\n";
  uint64_t written = 0;
  Status status = fs::FsWrite(
    g_boot_files, hello_file.value(),
    std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(kHello),
                             sizeof(kHello) - 1U),
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
  Log(LogLevel::kInfo, "fs: tmpfs write ok\n");
  return Status::kOk;
}

Status SetupWritableFs() noexcept {
  const Result<WritableTree> tree = CreateWritableTree();
  if (tree.is_err()) {
    return tree.status();
  }
  Status status = MountWritableTree(tree.value());
  if (IsErr(status)) {
    return status;
  }
  status = SeedEtcConfig(tree.value().etc);
  if (IsErr(status)) {
    return status;
  }
  status = VerifyTmpWrite();
  if (IsErr(status)) {
    return status;
  }
  Log(LogLevel::kInfo, "fs: writable mounts ok\n");
  return Status::kOk;
}

Status MountFat32(const fs::BlockDevice& disk) noexcept {
  const Result<fs::Partition> partition = fs::FindEspPartition(disk);
  if (partition.is_err()) {
    return partition.status();
  }
  const fs::BlockDevice device{&BlockReadAdapter, disk.context,
                               partition.value().sector_count,
                               partition.value().start_sector,
                               &BlockWriteAdapter};
  fs::Node* fat_root = nullptr;
  Status status = fs::fat32::Mount(device, &fat_root);
  if (IsErr(status)) {
    return status;
  }
  status = fs::FsMount(gsl::layout::kBootVolume, fat_root);
  if (IsErr(status)) {
    return status;
  }

  const Result<uint32_t> program =
    fs::FsOpen(g_boot_files, "/volumes/boot/EFI/BOOT/BOOTAA64.EFI");
  if (program.is_err()) {
    return program.status();
  }
  uint8_t header[2] = {0, 0};
  uint64_t read = 0;
  status = fs::FsRead(g_boot_files, program.value(),
                      std::span<uint8_t>(header, sizeof(header)), read);
  if (IsErr(status) || read != sizeof(header)) {
    (void)fs::FsClose(g_boot_files, program.value());
    return Status::kIoError;
  }
  if (header[0] != 'M' || header[1] != 'Z') {
    (void)fs::FsClose(g_boot_files, program.value());
    return Status::kIoError;
  }

  const Result<fs::Stat> stat =
    fs::FsStat("/volumes/boot/EFI/BOOT/BOOTAA64.EFI");
  if (stat.is_ok()) {
    LogDec(LogLevel::kInfo, "fs: efi image bytes", stat.value().size);
  }
  (void)fs::FsClose(g_boot_files, program.value());
  Log(LogLevel::kInfo, "fs: fat32 file read ok\n");
  return Status::kOk;
}

const char* ProcessStateName(process::ProcessState state) noexcept {
  switch (state) {
    case process::ProcessState::kReady:
      return "ready";
    case process::ProcessState::kExited:
      return "exited";
    case process::ProcessState::kReaping:
      return "reaping";
    default:
      return "unknown";
  }
}

Status FormatProcesses(char* buffer, std::size_t capacity,
                       std::size_t& length) noexcept {
  og::fmt::LineBuilder line(buffer, capacity);
  line.Text("pid ppid state name\n");
  process::ProcessInfo info[process::kMaxProcesses];
  const uint32_t count =
    process::Snapshot(info, process::kMaxProcesses);
  for (uint32_t index = 0; index < count; ++index) {
    line.Dec(info[index].pid).Char(' ')
        .Dec(info[index].parent_pid).Char(' ')
        .Text(ProcessStateName(info[index].state)).Char(' ')
        .Text(info[index].name).Char('\n');
  }
  length = line.size();
  return Status::kOk;
}

Status MountProcfs() noexcept {
  Result<fs::Node*> proc =
    fs::procfs::Create(&sched::ThreadCount, &FormatProcesses);
  if (proc.is_err()) {
    return proc.status();
  }
  const Status status = fs::FsMount(gsl::layout::kProc, proc.value());
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
  Log(LogLevel::kInfo, "proc: uptime ");
  Log(LogLevel::kInfo, text);
  return Status::kOk;
}

[[nodiscard]] Result<drivers::virtio::Blk*> FindBlockDevice() noexcept {
  const Result<drivers::Device> found =
    drivers::FindDeviceByName("virtio-blk0");
  if (found.is_err() || found.value().driver_data == nullptr) {
    Log(LogLevel::kWarn, "fs: no block device\n");
    return Result<drivers::virtio::Blk*>::Err(Status::kNoDevice);
  }
  auto* block = static_cast<drivers::virtio::Blk*>(found.value().driver_data);
  if (!block->is_ready()) {
    Log(LogLevel::kWarn, "fs: block device not ready\n");
    return Result<drivers::virtio::Blk*>::Err(Status::kNotInit);
  }
  return Result<drivers::virtio::Blk*>::Ok(block);
}

[[nodiscard]] Status InitFileTables() noexcept {
  Status status = fs::FsInit();
  if (IsErr(status)) {
    Log(LogLevel::kError, "fs: init failed\n");
    return status;
  }
  status = fs::FileTableInit(g_boot_files);
  if (IsErr(status)) {
    Log(LogLevel::kError, "fs: boot file table failed\n");
    return status;
  }
  return Status::kOk;
}

[[nodiscard]] Status MountSystemVolume(const fs::BlockDevice& disk) noexcept {
  const Result<fs::Partition> system_partition =
    fs::FindPartitionByMagic(disk, fs::gslfs::kMagic,
                             static_cast<uint32_t>(sizeof(fs::gslfs::kMagic)));
  if (system_partition.is_err()) {
    Log(LogLevel::kError, "fs: no gslfs partition found\n");
    return system_partition.status();
  }
  const fs::BlockDevice system_device{
    &BlockReadAdapter, disk.context, system_partition.value().sector_count,
    system_partition.value().start_sector};
  const Result<fs::Node*> system_root = fs::gslfs::Mount(system_device);
  if (system_root.is_err()) {
    Log(LogLevel::kError, "fs: gslfs mount failed\n");
    return system_root.status();
  }
  const Status status = fs::FsMount(gsl::layout::kRoot, system_root.value());
  if (IsErr(status)) {
    Log(LogLevel::kError, "fs: root mount failed\n");
    return status;
  }
  Log(LogLevel::kInfo, "fs: gslfs mounted\n");
  return Status::kOk;
}

void ProbeLibrary() noexcept {
  const Result<fs::Stat> library_stat = fs::FsStat(gsl::layout::kLibGemapi);
  if (library_stat.is_ok()) {
    LogDec(LogLevel::kInfo, "fs: libgemapi bytes", library_stat.value().size);
  } else {
    LogDec(LogLevel::kError, "fs: libgemapi stat status",
           static_cast<uint64_t>(library_stat.status()));
  }

  const Result<uint32_t> library_file =
    fs::FsOpen(g_boot_files, gsl::layout::kLibGemapi);
  if (library_file.is_err()) {
    LogDec(LogLevel::kError, "fs: library open status",
           static_cast<uint64_t>(library_file.status()));
    return;
  }

  uint8_t magic[4] = {};
  uint64_t read = 0;
  const Status status = fs::FsRead(
    g_boot_files, library_file.value(),
    std::span<uint8_t>(magic, sizeof(magic)), read);
  if (IsOk(status) && read == sizeof(magic) && magic[0] == 0x7FU &&
      magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'F') {
    Log(LogLevel::kInfo, "fs: library probe ok\n");
  } else {
    Log(LogLevel::kError, "fs: library probe failed\n");
  }
  (void)fs::FsClose(g_boot_files, library_file.value());
}

[[nodiscard]] Status MountDevfs() noexcept {
  const Result<fs::Node*> dev_root = fs::devfs::Create();
  if (dev_root.is_err()) {
    Log(LogLevel::kError, "fs: devfs create failed\n");
    return dev_root.status();
  }
  const Status status = fs::FsMount(gsl::layout::kDev, dev_root.value());
  if (IsErr(status)) {
    Log(LogLevel::kError, "fs: devfs mount failed\n");
    return status;
  }
  return Status::kOk;
}

void MountOptionalFilesystems(const fs::BlockDevice& disk) noexcept {
  if (IsErr(MountProcfs())) {
    Log(LogLevel::kWarn, "fs: procfs setup failed\n");
  }
  if (IsErr(SetupWritableFs())) {
    Log(LogLevel::kWarn, "fs: writable mounts failed\n");
  }
  if (IsErr(MountFat32(disk))) {
    Log(LogLevel::kWarn, "fs: fat32 setup failed\n");
  }
  Log(LogLevel::kInfo, "fs: mounted gslfs, devfs, procfs, writable, fat32\n");
}

void ProbeConsole() noexcept {
  const Result<uint32_t> console = fs::FsOpen(g_boot_files, "/dev/console");
  if (console.is_err()) {
    Log(LogLevel::kError, "fs: open dev console failed\n");
    return;
  }
  constexpr char kMessage[] = "fs: devfs console ok\n";
  uint64_t written = 0;
  (void)fs::FsWrite(
    g_boot_files, console.value(),
    std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(kMessage),
                             sizeof(kMessage) - 1U),
    written);
  (void)fs::FsClose(g_boot_files, console.value());
}

void ListRoot() noexcept {
  for (uint32_t index = 0; index < 16U; ++index) {
    fs::DirEntry entry{};
    bool found = false;
    const Status status = fs::FsList("/", index, entry, found);
    if (IsErr(status) || !found) {
      break;
    }
    (void)PrintLn(entry.name);
  }
}

}  // namespace

void InitFilesystem() noexcept {
  const Result<drivers::virtio::Blk*> block = FindBlockDevice();
  if (block.is_err()) {
    return;
  }
  if (IsErr(InitFileTables())) {
    return;
  }

  const fs::BlockDevice disk{&BlockReadAdapter, block.value(),
                             block.value()->capacity_sectors(), 0};

  if (IsErr(MountSystemVolume(disk))) {
    return;
  }

  ProbeLibrary();

  if (IsErr(MountDevfs())) {
    return;
  }

  MountOptionalFilesystems(disk);

  ProbeConsole();

  ListRoot();
}

}  // namespace og::boot
