#include "og/fs/procfs.h"

#include "format.h"
#include "memory.h"
#include "og/acpi/acpi.h"
#include "og/core/log_ring.h"
#include "og/time/timer.h"

namespace og::fs::procfs {

namespace {

enum class ProcKind : uint32_t {
  kUptime = 0,
  kThreads = 1,
  kAcpi = 2,
  kProcesses = 3,
  kDmesg = 4,
  kBlock = 5,
  kMeminfo = 6,
  kStat = 7,
  kTrace = 8,
  kDevices = 9,
  kInterrupts = 10,
  kMounts = 11,
};

struct ProcFile {
  Node node;
  char name[16];
  ProcKind kind;
};

struct ProcDir {
  Node node;
  ProcFile files[12];
  uint32_t count;
};

ProcDir g_root;
ThreadCountFn g_thread_count = nullptr;
ProcessTextFn g_process_text = nullptr;
BlockTextFn g_block_text = nullptr;
TextFn g_meminfo_text = nullptr;
TextFn g_stat_text = nullptr;
TextFn g_trace_text = nullptr;
TextFn g_devices_text = nullptr;
TextFn g_interrupts_text = nullptr;
TextFn g_mounts_text = nullptr;
TextFn g_threads_text = nullptr;

void GenerateAcpi(og::fmt::LineBuilder& line) noexcept {
  const acpi::Info& info = acpi::GetInfo();
  if (!info.ready) {
    line.Text("ACPI unavailable\n");
    return;
  }
  line.Text("ACPI revision ").Dec(info.revision).Char('\n');
  line.Text("rsdp ").Hex(info.rsdp_address).Char('\n');
  line.Text("root ").Hex(info.root_table_address).Char('\n');
  line.Text("tables ").Dec(info.table_count).Char('\n');
  for (uint32_t index = 0; index < info.table_count; ++index) {
    const Result<acpi::Table> table = acpi::TableAt(index);
    if (table.is_err()) {
      continue;
    }
    line.Text("  ").Text(table.value().signature).Text(" len ")
      .Dec(table.value().length).Char('\n');
  }
  line.Text("MADT cpus ").Dec(info.cpu_count).Char('\n');
  if (info.gic_distributor != 0U) {
    line.Text("GICD ").Hex(info.gic_distributor).Char('\n');
  }
  if (info.gic_redistributor != 0U) {
    line.Text("GICR ").Hex(info.gic_redistributor).Char('\n');
  }
  if (info.uart_base != 0U) {
    line.Text("SPCR uart ").Hex(info.uart_base).Char('\n');
  }
  if (info.pci_ecam != 0U) {
    line.Text("MCFG ecam ").Hex(info.pci_ecam).Text(" size ")
      .Hex(info.pci_ecam_size).Char('\n');
  }
  line.Text("GTDT ").Dec(info.has_gtdt ? 1U : 0U).Char('\n');
  line.Text("FADT flags ").Hex(info.fadt_flags).Char('\n');
  line.Text("ARM boot ").Hex(info.arm_boot_flags).Char('\n');
  line.Text("PSCI ").Dec(info.psci_compliant ? 1U : 0U).Text(" hvc ")
    .Dec(info.psci_use_hvc ? 1U : 0U).Char('\n');
  line.Text("reset reg ").Dec(info.reset_register_supported ? 1U : 0U)
    .Char('\n');
}

Status Generate(const ProcFile& file, char* buffer, std::size_t capacity,
                std::size_t& length) noexcept {
  og::fmt::LineBuilder line(buffer, capacity);
  switch (file.kind) {
    case ProcKind::kUptime:
      line.Dec(og::time::TimeNowMs());
      line.Text(" ms\n");
      break;
    case ProcKind::kThreads:
      if (g_threads_text == nullptr) {
        line.Dec(g_thread_count == nullptr ? 0U : g_thread_count());
        line.Text("\n");
      } else {
        return g_threads_text(buffer, capacity, length);
      }
      break;
    case ProcKind::kAcpi:
      GenerateAcpi(line);
      break;
    case ProcKind::kProcesses:
      if (g_process_text == nullptr) {
        line.Text("no processes\n");
      } else {
        return g_process_text(buffer, capacity, length);
      }
      break;
    case ProcKind::kDmesg:
      break;
    case ProcKind::kBlock:
      if (g_block_text == nullptr) {
        line.Text("no block devices\n");
      } else {
        return g_block_text(buffer, capacity, length);
      }
      break;
    case ProcKind::kMeminfo:
      if (g_meminfo_text == nullptr) {
        line.Text("meminfo unavailable\n");
      } else {
        return g_meminfo_text(buffer, capacity, length);
      }
      break;
    case ProcKind::kStat:
      if (g_stat_text == nullptr) {
        line.Text("stat unavailable\n");
      } else {
        return g_stat_text(buffer, capacity, length);
      }
      break;
    case ProcKind::kTrace:
      if (g_trace_text == nullptr) {
        line.Text("trace unavailable\n");
      } else {
        return g_trace_text(buffer, capacity, length);
      }
      break;
    case ProcKind::kDevices:
      if (g_devices_text == nullptr) {
        line.Text("devices unavailable\n");
      } else {
        return g_devices_text(buffer, capacity, length);
      }
      break;
    case ProcKind::kInterrupts:
      if (g_interrupts_text == nullptr) {
        line.Text("interrupts unavailable\n");
      } else {
        return g_interrupts_text(buffer, capacity, length);
      }
      break;
    case ProcKind::kMounts:
      if (g_mounts_text == nullptr) {
        line.Text("mounts unavailable\n");
      } else {
        return g_mounts_text(buffer, capacity, length);
      }
      break;
    default:
      return Status::kInvalidArg;
  }
  length = line.size();
  return Status::kOk;
}

Status FileRead(Node* node, uint64_t offset, std::span<uint8_t> buffer,
                uint64_t& out_read) noexcept {
  out_read = 0;
  if (node == nullptr) {
    return Status::kInvalidArg;
  }
  if (buffer.empty()) {
    return Status::kInvalidArg;
  }
  auto* file = static_cast<ProcFile*>(node->fs_data);
  if (file == nullptr) {
    return Status::kNotInit;
  }

  if (file->kind == ProcKind::kDmesg) {
    const uint32_t capacity =
      buffer.size() > 0xFFFFFFFFULL
        ? 0xFFFFFFFFU
        : static_cast<uint32_t>(buffer.size());
    out_read = log_ring::Read(
      offset, reinterpret_cast<char*>(buffer.data()), capacity);
    return Status::kOk;
  }

  char text[1024];
  std::size_t length = 0;
  const Status status = Generate(*file, text, sizeof(text), length);
  if (IsErr(status)) {
    return status;
  }
  if (offset >= length) {
    return Status::kOk;
  }
  uint64_t available = length - offset;
  if (available > buffer.size()) {
    available = buffer.size();
  }
  mem::Copy(buffer.data(), text + offset, available);
  out_read = available;
  return Status::kOk;
}

Status DirLookup(Node* /*directory*/, const char* name, Node** out) noexcept {
  if (out == nullptr) {
    return Status::kInvalidArg;
  }
  *out = nullptr;
  if (name == nullptr) {
    return Status::kInvalidArg;
  }
  for (uint32_t index = 0; index < g_root.count; ++index) {
    if (mem::Same(g_root.files[index].name, name)) {
      *out = &g_root.files[index].node;
      return Status::kOk;
    }
  }
  return Status::kNotFound;
}

Status DirIterate(Node* /*directory*/, uint32_t index, DirEntry& entry,
                  bool& found) noexcept {
  found = false;
  if (index >= g_root.count) {
    return Status::kOk;
  }
  const ProcFile& file = g_root.files[index];
  mem::Zero(&entry, sizeof(entry));
  const std::size_t length = mem::Length(file.name);
  if (length >= sizeof(entry.name)) {
    return Status::kInvalidArg;
  }
  mem::Copy(entry.name, file.name, length + 1U);
  entry.type = NodeType::kFile;
  entry.size = 0;
  found = true;
  return Status::kOk;
}

const NodeOps kFileOps{nullptr, &FileRead, nullptr, nullptr};
const NodeOps kDirOps{&DirLookup, nullptr, nullptr, &DirIterate};

void AddFile(uint32_t index, const char* name, ProcKind kind) noexcept {
  ProcFile& file = g_root.files[index];
  mem::Zero(&file, sizeof(file));
  const std::size_t length = mem::Length(name);
  mem::Copy(file.name, name, length + 1U);
  file.node.name = file.name;
  file.node.type = NodeType::kFile;
  file.node.mode = kModeReadOnlyFile;
  file.node.size = 0;
  file.node.ops = &kFileOps;
  file.node.fs_data = &file;
  file.kind = kind;
}

}  // namespace

Result<Node*> Create(ThreadCountFn thread_count,
                     ProcessTextFn process_text,
                     BlockTextFn block_text,
                     TextFn meminfo_text,
                     TextFn stat_text,
                     TextFn trace_text,
                     TextFn devices_text,
                     TextFn interrupts_text,
                     TextFn mounts_text,
                     TextFn threads_text) noexcept {
  g_thread_count = thread_count;
  g_process_text = process_text;
  g_block_text = block_text;
  g_meminfo_text = meminfo_text;
  g_stat_text = stat_text;
  g_trace_text = trace_text;
  g_devices_text = devices_text;
  g_interrupts_text = interrupts_text;
  g_mounts_text = mounts_text;
  g_threads_text = threads_text;
  mem::Zero(&g_root, sizeof(g_root));
  g_root.node.name = "/";
  g_root.node.type = NodeType::kDirectory;
  g_root.node.mode = kModeDirectory;
  g_root.node.size = 0;
  g_root.node.ops = &kDirOps;
  g_root.node.fs_data = &g_root;
  g_root.count = 0;

  AddFile(0, "uptime", ProcKind::kUptime);
  AddFile(1, "threads", ProcKind::kThreads);
  AddFile(2, "acpi", ProcKind::kAcpi);
  AddFile(3, "processes", ProcKind::kProcesses);
  AddFile(4, "dmesg", ProcKind::kDmesg);
  AddFile(5, "block", ProcKind::kBlock);
  AddFile(6, "meminfo", ProcKind::kMeminfo);
  AddFile(7, "stat", ProcKind::kStat);
  AddFile(8, "trace", ProcKind::kTrace);
  AddFile(9, "devices", ProcKind::kDevices);
  AddFile(10, "interrupts", ProcKind::kInterrupts);
  AddFile(11, "mounts", ProcKind::kMounts);
  g_root.count = 12;
  return Result<Node*>::Ok(&g_root.node);
}

}  // namespace og::fs::procfs
