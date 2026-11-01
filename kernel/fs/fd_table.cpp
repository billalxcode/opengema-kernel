#include "og/fs/vfs.h"

#include "memory.h"
#include "og/fs/vfs_internal.h"

// File descriptor table only. Path resolution and mounting stay in vfs.cpp.
namespace og::fs {

Status FileTableInit(FileTable& table) noexcept {
  mem::Zero(&table, sizeof(table));
  for (uint32_t index = 0; index < kFirstFileDescriptor; ++index) {
    table.entries[index].in_use = true;
  }
  return Status::kOk;
}

Result<uint32_t> FsOpen(FileTable& table, const char* path,
                        uint32_t flags) noexcept {
  if (!IsReady()) {
    return Result<uint32_t>::Err(Status::kNotInit);
  }
  constexpr uint32_t kKnownFlags =
    kOpenWrite | kOpenAppend | kOpenCreate | kOpenTruncate;
  if ((flags & ~kKnownFlags) != 0U) {
    return Result<uint32_t>::Err(Status::kInvalidArg);
  }
  if ((flags & (kOpenCreate | kOpenTruncate)) != 0U) {
    return Result<uint32_t>::Err(Status::kUnsupported);
  }
  Result<Node*> node = FsResolve(path);
  if (node.is_err()) {
    return Result<uint32_t>::Err(node.status());
  }

  for (uint32_t index = kFirstFileDescriptor; index < kMaxFiles; ++index) {
    if (table.entries[index].in_use) {
      continue;
    }
    table.entries[index].node = node.value();
    table.entries[index].offset = 0;
    table.entries[index].flags = flags;
    table.entries[index].in_use = true;
    return Result<uint32_t>::Ok(index);
  }
  return Result<uint32_t>::Err(Status::kNoMem);
}

Status FsClose(FileTable& table, uint32_t descriptor) noexcept {
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
  table.entries[descriptor].node = nullptr;
  table.entries[descriptor].offset = 0;
  table.entries[descriptor].flags = 0;
  table.entries[descriptor].in_use = false;
  return Status::kOk;
}

Status FsRead(FileTable& table, uint32_t descriptor, std::span<uint8_t> buffer,
              uint64_t& out_read) noexcept {
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
