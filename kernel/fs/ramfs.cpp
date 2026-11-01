#include "og/fs/ramfs.h"

#include "memory.h"
#include "og/mem/heap.h"

namespace og::fs::ramfs {

namespace {

constexpr uint32_t kMaxFiles = 8;
constexpr uint32_t kMaxName = 32;

struct RamFile {
  Node node;
  char name[kMaxName];
  uint8_t* data;
  uint64_t size;
  uint64_t capacity;
};

struct RamDir {
  Node node;
  RamFile files[kMaxFiles];
  uint32_t count;
};

RamDir g_root;

Status FileRead(Node* node, uint64_t offset, std::span<uint8_t> buffer,
                uint64_t& out_read) noexcept {
  out_read = 0;
  if (node == nullptr) {
    return Status::kInvalidArg;
  }
  if (buffer.empty()) {
    return Status::kInvalidArg;
  }
  auto* file = static_cast<RamFile*>(node->fs_data);
  if (file == nullptr) {
    return Status::kNotInit;
  }
  if (offset >= file->size) {
    return Status::kOk;
  }

  uint64_t available = file->size - offset;
  if (available > buffer.size()) {
    available = buffer.size();
  }
  mem::Copy(buffer.data(), file->data + offset, available);
  out_read = available;
  return Status::kOk;
}

Status FileWrite(Node* node, uint64_t offset, std::span<const uint8_t> buffer,
                 uint64_t& out_written) noexcept {
  out_written = 0;
  if (node == nullptr) {
    return Status::kInvalidArg;
  }
  if (buffer.empty()) {
    return Status::kInvalidArg;
  }
  auto* file = static_cast<RamFile*>(node->fs_data);
  if (file == nullptr) {
    return Status::kNotInit;
  }

  const uint64_t end = offset + buffer.size();
  if (end < offset) {
    return Status::kInvalidArg;
  }

  if (end > file->capacity) {
    uint64_t capacity = file->capacity == 0 ? 64U : file->capacity;
    while (capacity < end) {
      capacity *= 2U;
    }
    const Result<void*> grown = mem::HeapAlloc(capacity);
    if (grown.is_err()) {
      return grown.status();
    }
    if (file->data != nullptr) {
      mem::Copy(grown.value(), file->data, file->size);
      (void)mem::HeapFree(file->data);
    }
    file->data = static_cast<uint8_t*>(grown.value());
    file->capacity = capacity;
  }

  mem::Copy(file->data + offset, buffer.data(), buffer.size());
  if (end > file->size) {
    file->size = end;
    node->size = end;
  }
  out_written = buffer.size();
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
  const RamFile& file = g_root.files[index];
  mem::Zero(&entry, sizeof(entry));
  const std::size_t length = mem::Length(file.name);
  if (length >= sizeof(entry.name)) {
    return Status::kInvalidArg;
  }
  mem::Copy(entry.name, file.name, length + 1U);
  entry.type = NodeType::kFile;
  entry.size = file.size;
  found = true;
  return Status::kOk;
}

const NodeOps kFileOps{nullptr, &FileRead, &FileWrite, nullptr};
const NodeOps kDirOps{&DirLookup, nullptr, nullptr, &DirIterate};

}  // namespace

Result<Node*> Create() noexcept {
  mem::Zero(&g_root, sizeof(g_root));
  g_root.node.name = "/";
  g_root.node.type = NodeType::kDirectory;
  g_root.node.size = 0;
  g_root.node.ops = &kDirOps;
  g_root.node.fs_data = &g_root;
  g_root.count = 0;
  return Result<Node*>::Ok(&g_root.node);
}

Result<Node*> CreateFile(Node* root, const char* name, const void* data,
                         uint64_t size) noexcept {
  if (root == nullptr) {
    return Result<Node*>::Err(Status::kInvalidArg);
  }
  if (name == nullptr) {
    return Result<Node*>::Err(Status::kInvalidArg);
  }
  if (size != 0 && data == nullptr) {
    return Result<Node*>::Err(Status::kInvalidArg);
  }
  if (g_root.count >= kMaxFiles) {
    return Result<Node*>::Err(Status::kNoMem);
  }

  const std::size_t length = mem::Length(name);
  if (length >= kMaxName) {
    return Result<Node*>::Err(Status::kInvalidArg);
  }

  RamFile* file = &g_root.files[g_root.count];
  mem::Zero(file, sizeof(*file));
  mem::Copy(file->name, name, length + 1U);
  file->node.name = file->name;
  file->node.type = NodeType::kFile;
  file->node.size = size;
  file->node.ops = &kFileOps;
  file->node.fs_data = file;

  if (size != 0) {
    const Result<void*> buffer = mem::HeapAlloc(size);
    if (buffer.is_err()) {
      return Result<Node*>::Err(buffer.status());
    }
    mem::Copy(buffer.value(), data, size);
    file->data = static_cast<uint8_t*>(buffer.value());
    file->capacity = size;
    file->size = size;
  }

  ++g_root.count;
  return Result<Node*>::Ok(&file->node);
}

}  // namespace og::fs::ramfs
