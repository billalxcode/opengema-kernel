#include "og/fs/vfs.h"

#include "memory.h"
#include "og/fs/vfs_internal.h"

namespace og::fs {

namespace {

struct MountEntry {
  char path[kMaxPath];
  Node* root;
};

MountEntry g_mounts[kMaxMounts];
uint32_t g_mount_count = 0;
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

[[nodiscard]] const MountEntry* FindBestMount(const char* path) noexcept {
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
  return found ? &g_mounts[best] : nullptr;
}

[[nodiscard]] Result<Node*> ResolveSegments(Node* root,
                                            const char* cursor) noexcept {
  Node* current = root;
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

struct ParentPath {
  Node* directory;
  char name[64];
};

[[nodiscard]] Result<ParentPath> ResolveParent(const char* path) noexcept {
  if (path == nullptr || path[0] != '/') {
    return Result<ParentPath>::Err(Status::kInvalidArg);
  }
  const std::size_t length = mem::Length(path);
  if (length <= 1U || length >= kMaxPath) {
    return Result<ParentPath>::Err(Status::kInvalidArg);
  }

  std::size_t slash = length;
  while (slash > 0U && path[slash - 1U] != '/') {
    --slash;
  }
  if (slash == 0U) {
    return Result<ParentPath>::Err(Status::kInvalidArg);
  }

  char parent_path[kMaxPath];
  if (slash == 1U) {
    parent_path[0] = '/';
    parent_path[1] = '\0';
  } else {
    mem::Copy(parent_path, path, slash - 1U);
    parent_path[slash - 1U] = '\0';
  }

  const char* name = path + slash;
  const std::size_t name_length = length - slash;
  if (name_length == 0U || name_length >= sizeof(ParentPath::name)) {
    return Result<ParentPath>::Err(Status::kInvalidArg);
  }

  const Result<Node*> directory = FsResolve(parent_path);
  if (directory.is_err()) {
    return Result<ParentPath>::Err(directory.status());
  }
  if (directory.value()->type != NodeType::kDirectory) {
    return Result<ParentPath>::Err(Status::kInvalidArg);
  }

  ParentPath parent{};
  parent.directory = directory.value();
  mem::Copy(parent.name, name, name_length);
  parent.name[name_length] = '\0';
  return Result<ParentPath>::Ok(parent);
}

[[nodiscard]] bool IsMountRoot(const char* path) noexcept {
  for (uint32_t index = 0; index < g_mount_count; ++index) {
    if (Equals(g_mounts[index].path, path)) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool IsReady() noexcept {
  return g_ready;
}

Status FsInit() noexcept {
  mem::Zero(g_mounts, sizeof(g_mounts));
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

  const MountEntry* mount = FindBestMount(path);
  if (mount == nullptr) {
    return Result<Node*>::Err(Status::kNotFound);
  }
  return ResolveSegments(mount->root, path + mem::Length(mount->path));
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

Result<Node*> FsCreate(const char* path, NodeType type) noexcept {
  if (path == nullptr || IsMountRoot(path)) {
    return Result<Node*>::Err(Status::kBusy);
  }
  const Result<ParentPath> parent = ResolveParent(path);
  if (parent.is_err()) {
    return Result<Node*>::Err(parent.status());
  }
  Node* directory = parent.value().directory;
  if (directory->ops == nullptr || directory->ops->create == nullptr) {
    return Result<Node*>::Err(Status::kUnsupported);
  }
  Node* created = nullptr;
  const Status status = directory->ops->create(
    directory, parent.value().name, type, &created);
  if (IsErr(status)) {
    return Result<Node*>::Err(status);
  }
  if (created == nullptr) {
    return Result<Node*>::Err(Status::kIoError);
  }
  return Result<Node*>::Ok(created);
}

Status FsRemove(const char* path, NodeType type) noexcept {
  if (path == nullptr || IsMountRoot(path)) {
    return Status::kBusy;
  }
  const Result<ParentPath> parent = ResolveParent(path);
  if (parent.is_err()) {
    return parent.status();
  }
  Node* directory = parent.value().directory;
  if (directory->ops == nullptr || directory->ops->lookup == nullptr ||
      directory->ops->remove == nullptr) {
    return Status::kUnsupported;
  }
  Node* node = nullptr;
  Status status = directory->ops->lookup(
    directory, parent.value().name, &node);
  if (IsErr(status)) {
    return status;
  }
  if (node == nullptr) {
    return Status::kNotFound;
  }
  if (node->type != type) {
    return Status::kInvalidArg;
  }
  return directory->ops->remove(directory, node);
}

Status FsRename(const char* old_path, const char* new_path) noexcept {
  if (old_path == nullptr || new_path == nullptr ||
      IsMountRoot(old_path) || IsMountRoot(new_path)) {
    return Status::kBusy;
  }
  const Result<ParentPath> old_parent = ResolveParent(old_path);
  if (old_parent.is_err()) {
    return old_parent.status();
  }
  const Result<ParentPath> new_parent = ResolveParent(new_path);
  if (new_parent.is_err()) {
    return new_parent.status();
  }
  Node* old_directory = old_parent.value().directory;
  Node* new_directory = new_parent.value().directory;
  if (old_directory->ops == nullptr || old_directory->ops->lookup == nullptr ||
      old_directory->ops->rename == nullptr ||
      old_directory->ops != new_directory->ops) {
    return Status::kUnsupported;
  }
  Node* node = nullptr;
  Status status = old_directory->ops->lookup(
    old_directory, old_parent.value().name, &node);
  if (IsErr(status)) {
    return status;
  }
  if (node == nullptr) {
    return Status::kNotFound;
  }
  return old_directory->ops->rename(
    old_directory, node, new_directory, new_parent.value().name);
}

}  // namespace og::fs
