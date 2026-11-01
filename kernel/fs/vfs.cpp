#include "og/fs/vfs.h"

#include "memory.h"

namespace og::fs {

namespace {

struct MountEntry {
  char path[kMaxPath];
  Node* root;
};

struct FileEntry {
  Node* node;
  uint64_t offset;
  bool in_use;
};

MountEntry g_mounts[kMaxMounts];
uint32_t g_mount_count = 0;

FileEntry g_files[kMaxFiles];
bool g_ready = false;

[[nodiscard]] bool IsSeparator(char value) noexcept {
  return value == '/';
}

[[nodiscard]] bool StartsWith(const char* text, const char* prefix) noexcept {
  if (text == nullptr || prefix == nullptr) {
    return false;
  }
  while (*prefix != '\0') {
    if (*text != *prefix) {
      return false;
    }
    ++text;
    ++prefix;
  }
  return true;
}

[[nodiscard]] bool IsMountPrefix(const char* path, const char* mount) noexcept {
  if (!StartsWith(path, mount)) {
    return false;
  }
  if (mem::Length(mount) == 1U) {
    return true;  // root mount
  }
  const char next = path[mem::Length(mount)];
  return next == '\0' || IsSeparator(next);
}

[[nodiscard]] bool Equals(const char* lhs, const char* rhs) noexcept {
  return mem::Same(lhs, rhs);
}

[[nodiscard]] Status CopyPath(char* destination, const char* source) noexcept {
  if (destination == nullptr || source == nullptr) {
    return Status::kInvalidArg;
  }
  const std::size_t length = mem::Length(source);
  if (length >= kMaxPath) {
    return Status::kInvalidArg;
  }
  mem::Copy(destination, source, length + 1U);
  return Status::kOk;
}

}  // namespace

Status FsInit() noexcept {
  mem::Zero(g_mounts, sizeof(g_mounts));
  mem::Zero(g_files, sizeof(g_files));
  g_mount_count = 0;
  g_ready = true;
  return Status::kOk;
}

Status FsMount(const char* path, Node* root) noexcept {
  if (!g_ready) {
    return Status::kNotInit;
  }
  if (path == nullptr || root == nullptr) {
    return Status::kInvalidArg;
  }
  if (root->type != NodeType::kDirectory) {
    return Status::kInvalidArg;
  }
  if (g_mount_count >= kMaxMounts) {
    return Status::kNoMem;
  }
  for (uint32_t index = 0; index < g_mount_count; ++index) {
    if (Equals(g_mounts[index].path, path)) {
      return Status::kExists;
    }
  }
  const Status status = CopyPath(g_mounts[g_mount_count].path, path);
  if (IsErr(status)) {
    return status;
  }
  g_mounts[g_mount_count].root = root;
  ++g_mount_count;
  return Status::kOk;
}

Result<Node*> FsResolve(const char* path) noexcept {
  if (!g_ready) {
    return Result<Node*>::Err(Status::kNotInit);
  }
  if (path == nullptr) {
    return Result<Node*>::Err(Status::kInvalidArg);
  }
  if (g_mount_count == 0) {
    return Result<Node*>::Err(Status::kNotFound);
  }

  uint32_t best = 0;
  std::size_t best_length = 0;
  bool found = false;
  for (uint32_t index = 0; index < g_mount_count; ++index) {
    if (!IsMountPrefix(path, g_mounts[index].path)) {
      continue;
    }
    const std::size_t length = mem::Length(g_mounts[index].path);
    if (!found || length > best_length) {
      best = index;
      best_length = length;
      found = true;
    }
  }
  if (!found) {
    return Result<Node*>::Err(Status::kNotFound);
  }

  Node* current = g_mounts[best].root;
  const char* cursor = path + best_length;

  while (*cursor != '\0') {
    while (IsSeparator(*cursor)) {
      ++cursor;
    }
    if (*cursor == '\0') {
      break;
    }

    char segment[64];
    std::size_t length = 0;
    while (*cursor != '\0' && !IsSeparator(*cursor)) {
      if (length >= (sizeof(segment) - 1U)) {
        return Result<Node*>::Err(Status::kInvalidArg);
      }
      segment[length] = *cursor;
      ++length;
      ++cursor;
    }
    segment[length] = '\0';

    if (Equals(segment, ".")) {
      continue;
    }
    if (Equals(segment, "..")) {
      // Escaping the mount root is rejected rather than guessed.
      return Result<Node*>::Err(Status::kInvalidArg);
    }
    if (current->type != NodeType::kDirectory) {
      return Result<Node*>::Err(Status::kNotFound);
    }
    if (current->ops == nullptr || current->ops->lookup == nullptr) {
      return Result<Node*>::Err(Status::kUnsupported);
    }

    Node* next = nullptr;
    const Status status = current->ops->lookup(current, segment, &next);
    if (IsErr(status)) {
      return Result<Node*>::Err(status);
    }
    if (next == nullptr) {
      return Result<Node*>::Err(Status::kNotFound);
    }
    current = next;
  }

  return Result<Node*>::Ok(current);
}

Result<uint32_t> FsOpen(const char* path) noexcept {
  Result<Node*> node = FsResolve(path);
  if (node.is_err()) {
    return Result<uint32_t>::Err(node.status());
  }

  for (uint32_t index = 0; index < kMaxFiles; ++index) {
    if (g_files[index].in_use) {
      continue;
    }
    g_files[index].node = node.value();
    g_files[index].offset = 0;
    g_files[index].in_use = true;
    return Result<uint32_t>::Ok(index);
  }
  return Result<uint32_t>::Err(Status::kNoMem);
}

Status FsClose(uint32_t descriptor) noexcept {
  if (!g_ready) {
    return Status::kNotInit;
  }
  if (descriptor >= kMaxFiles) {
    return Status::kInvalidArg;
  }
  if (!g_files[descriptor].in_use) {
    return Status::kNotFound;
  }
  g_files[descriptor].node = nullptr;
  g_files[descriptor].offset = 0;
  g_files[descriptor].in_use = false;
  return Status::kOk;
}

Status FsRead(uint32_t descriptor, std::span<uint8_t> buffer, uint64_t& out_read) noexcept {
  out_read = 0;
  if (!g_ready) {
    return Status::kNotInit;
  }
  if (descriptor >= kMaxFiles) {
    return Status::kInvalidArg;
  }
  if (!g_files[descriptor].in_use) {
    return Status::kNotFound;
  }
  if (buffer.empty()) {
    return Status::kInvalidArg;
  }
  Node* node = g_files[descriptor].node;
  if (node == nullptr || node->ops == nullptr || node->ops->read == nullptr) {
    return Status::kUnsupported;
  }

  const Status status = node->ops->read(node, g_files[descriptor].offset, buffer, out_read);
  if (IsErr(status)) {
    return status;
  }
  g_files[descriptor].offset += out_read;
  return Status::kOk;
}

Status FsWrite(uint32_t descriptor, std::span<const uint8_t> buffer,
               uint64_t& out_written) noexcept {
  out_written = 0;
  if (!g_ready) {
    return Status::kNotInit;
  }
  if (descriptor >= kMaxFiles) {
    return Status::kInvalidArg;
  }
  if (!g_files[descriptor].in_use) {
    return Status::kNotFound;
  }
  if (buffer.empty()) {
    return Status::kInvalidArg;
  }
  Node* node = g_files[descriptor].node;
  if (node == nullptr || node->ops == nullptr || node->ops->write == nullptr) {
    return Status::kUnsupported;
  }

  const Status status = node->ops->write(node, g_files[descriptor].offset, buffer, out_written);
  if (IsErr(status)) {
    return status;
  }
  g_files[descriptor].offset += out_written;
  return Status::kOk;
}

Status FsSeek(uint32_t descriptor, uint64_t offset) noexcept {
  if (!g_ready) {
    return Status::kNotInit;
  }
  if (descriptor >= kMaxFiles) {
    return Status::kInvalidArg;
  }
  if (!g_files[descriptor].in_use) {
    return Status::kNotFound;
  }
  const Node* node = g_files[descriptor].node;
  if (node == nullptr) {
    return Status::kNotFound;
  }
  if (node->type == NodeType::kFile && offset > node->size) {
    return Status::kInvalidArg;
  }
  g_files[descriptor].offset = offset;
  return Status::kOk;
}

Result<Stat> FsStat(const char* path) noexcept {
  Result<Node*> node = FsResolve(path);
  if (node.is_err()) {
    return Result<Stat>::Err(node.status());
  }
  Stat stat{};
  stat.type = node.value()->type;
  stat.size = node.value()->size;
  return Result<Stat>::Ok(stat);
}

Status FsList(const char* path, uint32_t index, DirEntry& entry, bool& found) noexcept {
  found = false;
  Result<Node*> node = FsResolve(path);
  if (node.is_err()) {
    return node.status();
  }
  if (node.value()->type != NodeType::kDirectory) {
    return Status::kInvalidArg;
  }
  if (node.value()->ops == nullptr || node.value()->ops->iterate == nullptr) {
    return Status::kUnsupported;
  }
  return node.value()->ops->iterate(node.value(), index, entry, found);
}

}  // namespace og::fs
