#include "og/fs/vfs.h"

#include "memory.h"
#include "og/fs/vfs_internal.h"

// File descriptor table only. Path resolution and mounting stay in vfs.cpp.
namespace og::fs {

namespace {

[[nodiscard]] bool AllowsAccess(const Node& node, uint32_t uid, uint32_t gid,
                                bool write) noexcept {
  if (uid == 0U) {
    return true;
  }
  const uint32_t shift = uid == node.uid ? 6U : (gid == node.gid ? 3U : 0U);
  const uint32_t bit = write ? 2U : 4U;
  return (node.mode & (bit << shift)) != 0U;
}

}  // namespace

Status FileTableInit(FileTable& table) noexcept {
  mem::Zero(&table, sizeof(table));
  for (uint32_t index = 0; index < kFirstFileDescriptor; ++index) {
    table.entries[index].in_use = true;
  }
  return Status::kOk;
}

Result<uint32_t> FsOpen(FileTable& table, const char* path,
                        uint32_t flags) noexcept {
  OperationGuard guard;
  if (!IsReady()) {
    return Result<uint32_t>::Err(Status::kNotInit);
  }
  constexpr uint32_t kKnownFlags =
    kOpenWrite | kOpenAppend | kOpenCreate | kOpenTruncate;
  if ((flags & ~kKnownFlags) != 0U) {
    return Result<uint32_t>::Err(Status::kInvalidArg);
  }
  uint32_t mount_index = 0;
  Result<Node*> node = ResolveForOpen(path, mount_index);
  if (node.is_err() && (flags & kOpenCreate) != 0U &&
      node.status() == Status::kNotFound) {
    const Result<Node*> created = FsCreate(path, NodeType::kFile);
    if (created.is_err()) {
      return Result<uint32_t>::Err(created.status());
    }
    node = ResolveForOpen(path, mount_index);
  }
  if (node.is_err()) {
    return Result<uint32_t>::Err(node.status());
  }
  constexpr uint32_t kWriteFlags =
    kOpenWrite | kOpenAppend | kOpenCreate | kOpenTruncate;
  const bool write_access = (flags & kWriteFlags) != 0U;
  if (!AllowsAccess(*node.value(), table.uid, table.gid, write_access)) {
    return Result<uint32_t>::Err(Status::kDenied);
  }
  if ((flags & kOpenTruncate) != 0U) {
    const Status truncated = FsTruncate(path, 0);
    if (IsErr(truncated)) {
      return Result<uint32_t>::Err(truncated);
    }
  }

  for (uint32_t index = kFirstFileDescriptor; index < kMaxFiles; ++index) {
    if (table.entries[index].in_use) {
      continue;
    }
    table.entries[index].node = node.value();
    table.entries[index].offset = 0;
    table.entries[index].flags = flags;
    table.entries[index].mount = mount_index;
    table.entries[index].in_use = true;
    const Status tracked = TrackOpen(mount_index);
    if (IsErr(tracked)) {
      table.entries[index] = FileEntry{};
      return Result<uint32_t>::Err(tracked);
    }
    return Result<uint32_t>::Ok(index);
  }
  return Result<uint32_t>::Err(Status::kNoMem);
}

Status FsClose(FileTable& table, uint32_t descriptor) noexcept {
  OperationGuard guard;
  if (!IsReady()) {
    return Status::kNotInit;
  }
  if (descriptor >= kMaxFiles) {
    return Status::kInvalidArg;
  }
  if (descriptor < kFirstFileDescriptor) {
    return Status::kUnsupported;
  }
  if (!table.entries[descriptor].in_use) {
    return Status::kNotFound;
  }
  const uint32_t mount_index = table.entries[descriptor].mount;
  table.entries[descriptor] = FileEntry{};
  UntrackOpen(mount_index);
  return Status::kOk;
}

Status FsRead(FileTable& table, uint32_t descriptor, std::span<uint8_t> buffer,
              uint64_t& out_read) noexcept {
  OperationGuard guard;
  out_read = 0;
  if (!IsReady()) {
    return Status::kNotInit;
  }
  if (descriptor >= kMaxFiles) {
    return Status::kInvalidArg;
  }
  if (descriptor < kFirstFileDescriptor) {
    return Status::kUnsupported;
  }
  if (!table.entries[descriptor].in_use) {
    return Status::kNotFound;
  }
  if (buffer.empty()) {
    return Status::kInvalidArg;
  }
  Node* node = table.entries[descriptor].node;
  if (node == nullptr || node->ops == nullptr || node->ops->read == nullptr) {
    return Status::kUnsupported;
  }

  const Status status =
    node->ops->read(node, table.entries[descriptor].offset, buffer, out_read);
  if (IsErr(status)) {
    return status;
  }
  table.entries[descriptor].offset += out_read;
  return Status::kOk;
}

Status FsWrite(FileTable& table, uint32_t descriptor,
               std::span<const uint8_t> buffer, uint64_t& out_written) noexcept {
  OperationGuard guard;
  out_written = 0;
  if (!IsReady()) {
    return Status::kNotInit;
  }
  if (descriptor >= kMaxFiles) {
    return Status::kInvalidArg;
  }
  if (descriptor < kFirstFileDescriptor) {
    return Status::kUnsupported;
  }
  if (!table.entries[descriptor].in_use) {
    return Status::kNotFound;
  }
  if (buffer.empty()) {
    return Status::kInvalidArg;
  }
  const uint32_t flags = table.entries[descriptor].flags;
  if ((flags & (kOpenWrite | kOpenAppend)) == 0U) {
    return Status::kUnsupported;
  }
  Node* node = table.entries[descriptor].node;
  if (node == nullptr || node->ops == nullptr || node->ops->write == nullptr) {
    return Status::kUnsupported;
  }

  uint64_t offset = table.entries[descriptor].offset;
  if ((flags & kOpenAppend) != 0U) {
    offset = node->size;
  }
  const Status status =
    node->ops->write(node, offset, buffer, out_written);
  if (IsErr(status)) {
    return status;
  }
  table.entries[descriptor].offset = offset + out_written;
  return Status::kOk;
}

Status FsSeek(FileTable& table, uint32_t descriptor, uint64_t offset) noexcept {
  OperationGuard guard;
  if (!IsReady()) {
    return Status::kNotInit;
  }
  if (descriptor >= kMaxFiles) {
    return Status::kInvalidArg;
  }
  if (descriptor < kFirstFileDescriptor) {
    return Status::kUnsupported;
  }
  if (!table.entries[descriptor].in_use) {
    return Status::kNotFound;
  }
  const Node* node = table.entries[descriptor].node;
  if (node == nullptr) {
    return Status::kNotFound;
  }
  if (node->type == NodeType::kFile && offset > node->size) {
    return Status::kInvalidArg;
  }
  table.entries[descriptor].offset = offset;
  return Status::kOk;
}

}  // namespace og::fs
