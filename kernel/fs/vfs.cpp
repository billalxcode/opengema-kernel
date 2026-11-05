#include "og/fs/vfs.h"

#include "memory.h"
#include "og/fs/vfs_internal.h"

#ifndef OG_HOST_TEST
#include "og/arch/spinlock.h"
#endif

namespace og::fs {

namespace {

struct MountEntry {
  char path[kMaxPath];
  Node* root;
  uint32_t refs;
  uint32_t open_files;
  bool used;
  bool mounted;
  bool pinned;
};

#ifdef OG_HOST_TEST
// Host probes have no IRQ state. The production build uses IrqSpinlock so a
// timer reschedule cannot switch into another mount table caller on the same
// CPU while the lock is held.
class HostSpinlock {
 public:
  void lock() noexcept {
    while (__atomic_exchange_n(&locked_, 1U, __ATOMIC_ACQUIRE) != 0U) {
    }
  }

  void unlock() noexcept {
    __atomic_store_n(&locked_, 0U, __ATOMIC_RELEASE);
  }

 private:
  uint32_t locked_ = 0;
};

using VfsLock = HostSpinlock;
#else
using VfsLock = arch::aarch64::IrqSpinlock;
#endif

class LockGuard {
 public:
  explicit LockGuard(VfsLock& lock) noexcept : lock_(lock) {
    lock_.lock();
  }

  ~LockGuard() noexcept {
    lock_.unlock();
  }

  LockGuard(const LockGuard&) = delete;
  LockGuard& operator=(const LockGuard&) = delete;

 private:
  VfsLock& lock_;
};

MountEntry g_mounts[kMaxMounts];
VfsLock g_lock;
bool g_ready = false;
uint32_t g_operations = 0;
uint64_t g_operation_total = 0;

// Releases roots whose mount name and open descriptors are both gone. The
// VFS lock is dropped before calling into a filesystem, keeping it a leaf.
void ReleasePending() noexcept {
  Node* roots[kMaxMounts]{};
  uint32_t count = 0;
  {
    LockGuard guard(g_lock);
    if (g_operations != 0U) {
      return;
    }
    for (uint32_t index = 0; index < kMaxMounts; ++index) {
      MountEntry& entry = g_mounts[index];
      if (!entry.used || entry.mounted || entry.refs != 0U ||
          entry.open_files != 0U) {
        continue;
      }
      roots[count] = entry.root;
      ++count;
      mem::Zero(&entry, sizeof(entry));
    }
  }
  for (uint32_t index = 0; index < count; ++index) {
    Node* root = roots[index];
    if (root != nullptr && root->ops != nullptr &&
        root->ops->release != nullptr) {
      root->ops->release(root);
    }
  }
}

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

[[nodiscard]] bool FindBestMountLocked(const char* path,
                                       uint32_t& out_index) noexcept {
  uint32_t best = 0;
  std::size_t best_length = 0;
  bool found = false;
  for (uint32_t index = 0; index < kMaxMounts; ++index) {
    if (!g_mounts[index].used || !g_mounts[index].mounted ||
        !IsMountPrefix(path, g_mounts[index].path)) {
      continue;
    }
    const std::size_t length = mem::Length(g_mounts[index].path);
    if (!found || length > best_length) {
      best = index;
      best_length = length;
      found = true;
    }
  }
  if (found) {
    out_index = best;
  }
  return found;
}

[[nodiscard]] bool FindMountByPathLocked(const char* path,
                                         uint32_t& out_index) noexcept {
  for (uint32_t index = 0; index < kMaxMounts; ++index) {
    if (g_mounts[index].used && g_mounts[index].mounted &&
        Equals(g_mounts[index].path, path)) {
      out_index = index;
      return true;
    }
  }
  return false;
}

[[nodiscard]] bool FindFreeMountLocked(uint32_t& out_index) noexcept {
  for (uint32_t index = 0; index < kMaxMounts; ++index) {
    if (!g_mounts[index].used) {
      out_index = index;
      return true;
    }
  }
  return false;
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

}  // namespace

OperationGuard::OperationGuard() noexcept {
  LockGuard guard(g_lock);
  ++g_operations;
  ++g_operation_total;
}

OperationGuard::~OperationGuard() noexcept {
  {
    LockGuard guard(g_lock);
    if (g_operations > 0U) {
      --g_operations;
    }
  }
  ReleasePending();
}

Result<Node*> ResolveForOpen(const char* path,
                             uint32_t& mount_index) noexcept {
  if (path == nullptr) {
    return Result<Node*>::Err(Status::kInvalidArg);
  }

  Node* root = nullptr;
  const char* suffix = nullptr;
  uint32_t index = 0;
  {
    LockGuard guard(g_lock);
    if (!g_ready) {
      return Result<Node*>::Err(Status::kNotInit);
    }
    if (!FindBestMountLocked(path, index)) {
      return Result<Node*>::Err(Status::kNotFound);
    }
    root = g_mounts[index].root;
    suffix = path + mem::Length(g_mounts[index].path);
  }

  Result<Node*> result = ResolveSegments(root, suffix);
  if (result.is_ok()) {
    mount_index = index;
  }
  return result;
}

Status TrackOpen(uint32_t mount_index) noexcept {
  LockGuard guard(g_lock);
  if (!g_ready || mount_index >= kMaxMounts) {
    return Status::kInvalidArg;
  }
  MountEntry& entry = g_mounts[mount_index];
  if (!entry.used) {
    return Status::kInvalidArg;
  }
  ++entry.open_files;
  return Status::kOk;
}

void UntrackOpen(uint32_t mount_index) noexcept {
  {
    LockGuard guard(g_lock);
    if (mount_index < kMaxMounts) {
      MountEntry& entry = g_mounts[mount_index];
      if (entry.used && entry.open_files > 0U) {
        --entry.open_files;
      }
    }
  }
  ReleasePending();
}

bool IsReady() noexcept {
  return g_ready;
}

Status FsInit() noexcept {
  LockGuard guard(g_lock);
  mem::Zero(g_mounts, sizeof(g_mounts));
  g_operations = 0;
  g_operation_total = 0;
  g_ready = true;
  return Status::kOk;
}

uint64_t FsOperationCount() noexcept {
  LockGuard guard(g_lock);
  return g_operation_total;
}

Status FsMount(const char* path, Node* root) noexcept {
  if (path == nullptr || root == nullptr) {
    return Status::kInvalidArg;
  }
  if (root->type != NodeType::kDirectory) {
    return Status::kInvalidArg;
  }

  LockGuard guard(g_lock);
  if (!g_ready) {
    return Status::kNotInit;
  }
  uint32_t existing = 0;
  if (FindMountByPathLocked(path, existing)) {
    return Status::kExists;
  }
  uint32_t slot = 0;
  if (!FindFreeMountLocked(slot)) {
    return Status::kNoMem;
  }
  const Status status = CopyPath(g_mounts[slot].path, path);
  if (IsErr(status)) {
    return status;
  }
  g_mounts[slot].root = root;
  g_mounts[slot].refs = 1;
  g_mounts[slot].open_files = 0;
  g_mounts[slot].used = true;
  g_mounts[slot].mounted = true;
  g_mounts[slot].pinned = true;
  return Status::kOk;
}

Status FsMountRef(const char* path, Node* root) noexcept {
  if (path == nullptr) {
    return Status::kInvalidArg;
  }
  if (root != nullptr && root->type != NodeType::kDirectory) {
    return Status::kInvalidArg;
  }

  LockGuard guard(g_lock);
  if (!g_ready) {
    return Status::kNotInit;
  }

  uint32_t existing = 0;
  if (FindMountByPathLocked(path, existing)) {
    if (root != nullptr) {
      return Status::kExists;
    }
    if (g_mounts[existing].pinned) {
      return Status::kBusy;
    }
    ++g_mounts[existing].refs;
    return Status::kOk;
  }
  if (root == nullptr) {
    return Status::kInvalidArg;
  }

  uint32_t slot = 0;
  if (!FindFreeMountLocked(slot)) {
    return Status::kNoMem;
  }
  const Status status = CopyPath(g_mounts[slot].path, path);
  if (IsErr(status)) {
    return status;
  }
  g_mounts[slot].root = root;
  g_mounts[slot].refs = 1;
  g_mounts[slot].open_files = 0;
  g_mounts[slot].used = true;
  g_mounts[slot].mounted = true;
  g_mounts[slot].pinned = false;
  return Status::kOk;
}

Status FsUnmount(const char* path) noexcept {
  if (path == nullptr) {
    return Status::kInvalidArg;
  }

  {
    LockGuard guard(g_lock);
    if (!g_ready) {
      return Status::kNotInit;
    }
    uint32_t index = 0;
    if (!FindMountByPathLocked(path, index)) {
      return Status::kNotFound;
    }
    if (g_mounts[index].pinned) {
      return Status::kBusy;
    }
    if (g_mounts[index].refs > 1U) {
      --g_mounts[index].refs;
      return Status::kOk;
    }

    // Hide the name now. The root stays alive until the last open
    // descriptor is closed; ReleasePending frees it after that.
    g_mounts[index].refs = 0;
    g_mounts[index].mounted = false;
  }

  ReleasePending();
  return Status::kOk;
}

bool FsIsMountRoot(const char* path) noexcept {
  if (path == nullptr) {
    return false;
  }
  LockGuard guard(g_lock);
  uint32_t index = 0;
  return g_ready && FindMountByPathLocked(path, index);
}

Result<Node*> FsResolve(const char* path) noexcept {
  OperationGuard guard;
  uint32_t mount_index = 0;
  return ResolveForOpen(path, mount_index);
}

Result<Stat> FsStat(const char* path) noexcept {
  OperationGuard guard;
  Result<Node*> node = FsResolve(path);
  if (node.is_err()) {
    return Result<Stat>::Err(node.status());
  }
  Stat stat{};
  stat.type = node.value()->type;
  stat.size = node.value()->size;
  stat.mode = node.value()->mode;
  stat.uid = node.value()->uid;
  stat.gid = node.value()->gid;
  stat.mtime = node.value()->mtime;
  return Result<Stat>::Ok(stat);
}

Status FsList(const char* path, uint32_t index, DirEntry& entry, bool& found) noexcept {
  OperationGuard guard;
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
  OperationGuard guard;
  if (path == nullptr || FsIsMountRoot(path)) {
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
  OperationGuard guard;
  if (path == nullptr || FsIsMountRoot(path)) {
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
  OperationGuard guard;
  if (old_path == nullptr || new_path == nullptr ||
      FsIsMountRoot(old_path) || FsIsMountRoot(new_path)) {
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

Status FsTruncate(const char* path, uint64_t size) noexcept {
  OperationGuard guard;
  if (path == nullptr) {
    return Status::kInvalidArg;
  }
  const Result<Node*> node = FsResolve(path);
  if (node.is_err()) {
    return node.status();
  }
  if (node.value()->type != NodeType::kFile) {
    return Status::kInvalidArg;
  }
  if (node.value()->ops == nullptr || node.value()->ops->truncate == nullptr) {
    return Status::kUnsupported;
  }
  return node.value()->ops->truncate(node.value(), size);
}

Status FsChmod(const char* path, uint32_t mode) noexcept {
  OperationGuard guard;
  if (path == nullptr || mode > 0777U) {
    return Status::kInvalidArg;
  }
  const Result<Node*> node = FsResolve(path);
  if (node.is_err()) {
    return node.status();
  }
  if (node.value()->ops == nullptr || node.value()->ops->chmod == nullptr) {
    return Status::kUnsupported;
  }
  return node.value()->ops->chmod(node.value(), mode);
}

Status FsChown(const char* path, uint32_t uid, uint32_t gid) noexcept {
  OperationGuard guard;
  if (path == nullptr) {
    return Status::kInvalidArg;
  }
  const Result<Node*> node = FsResolve(path);
  if (node.is_err()) {
    return node.status();
  }
  if (node.value()->ops == nullptr || node.value()->ops->chown == nullptr) {
    return Status::kUnsupported;
  }
  return node.value()->ops->chown(node.value(), uid, gid);
}

}  // namespace og::fs
