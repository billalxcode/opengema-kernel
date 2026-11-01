#include "og/procfs.h"

#include "format.h"
#include "memory.h"
#include "og/timer.h"

namespace og::fs::procfs {

namespace {

enum class ProcKind : uint32_t {
  kUptime = 0,
  kThreads = 1,
};

struct ProcFile {
  Node node;
  char name[16];
  ProcKind kind;
};

struct ProcDir {
  Node node;
  ProcFile files[2];
  uint32_t count;
};

ProcDir g_root;
ThreadCountFn g_thread_count = nullptr;

Status Generate(const ProcFile& file, char* buffer, std::size_t capacity,
                std::size_t& length) noexcept {
  og::fmt::LineBuilder line(buffer, capacity);
  switch (file.kind) {
    case ProcKind::kUptime:
      line.Dec(og::time::TimeNowMs());
      line.Text(" ms\n");
      break;
    case ProcKind::kThreads:
      line.Dec(g_thread_count == nullptr ? 0U : g_thread_count());
      line.Text("\n");
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

  char text[64];
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
  file.node.size = 0;
  file.node.ops = &kFileOps;
  file.node.fs_data = &file;
  file.kind = kind;
}

}  // namespace

Result<Node*> Create(ThreadCountFn thread_count) noexcept {
  g_thread_count = thread_count;
  mem::Zero(&g_root, sizeof(g_root));
  g_root.node.name = "/";
  g_root.node.type = NodeType::kDirectory;
  g_root.node.size = 0;
  g_root.node.ops = &kDirOps;
  g_root.node.fs_data = &g_root;
  g_root.count = 0;

  AddFile(0, "uptime", ProcKind::kUptime);
  AddFile(1, "threads", ProcKind::kThreads);
  g_root.count = 2;
  return Result<Node*>::Ok(&g_root.node);
}

}  // namespace og::fs::procfs
