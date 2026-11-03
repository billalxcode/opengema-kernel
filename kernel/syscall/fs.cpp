#include "og/syscall/syscall_internal.h"

#include "memory.h"
#include "og/fs/ramfs.h"
#include "og/fs/vfs.h"
#include "og/mem/vmm.h"

namespace og::syscall {

using namespace detail;

namespace {

Status RemovePath(uint64_t user_path, fs::NodeType type,
                  uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  char path[fs::kMaxPath];
  const Status copy = CopyUserPath(*process, user_path, path);
  if (IsErr(copy)) {
    return copy;
  }
  const Status status = fs::FsRemove(path, type);
  if (IsErr(status)) {
    return status;
  }
  result = 0;
  return Status::kOk;
}

}  // namespace

Status DoOpen(uint64_t user_path, uint64_t flags, uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  char path[fs::kMaxPath];
  Status status = CopyUserPath(*process, user_path, path);
  if (IsErr(status)) {
    return status;
  }
  const Result<uint32_t> descriptor =
    fs::FsOpen(process->files, path, static_cast<uint32_t>(flags));
  if (descriptor.is_err()) {
    return descriptor.status();
  }
  result = descriptor.value();
  return Status::kOk;
}

Status DoClose(uint64_t descriptor, uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  const Status status =
    fs::FsClose(process->files, static_cast<uint32_t>(descriptor));
  if (IsErr(status)) {
    return status;
  }
  result = 0;
  return Status::kOk;
}

Status DoSeek(uint64_t descriptor, uint64_t offset, uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  const Status status =
    fs::FsSeek(process->files, static_cast<uint32_t>(descriptor), offset);
  if (IsErr(status)) {
    return status;
  }
  result = 0;
  return Status::kOk;
}

Status DoStat(uint64_t user_path, uint64_t user_stat,
              uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  char path[fs::kMaxPath];
  Status status = CopyUserPath(*process, user_path, path);
  if (IsErr(status)) {
    return status;
  }
  const Result<fs::Stat> stat = fs::FsStat(path);
  if (stat.is_err()) {
    return stat.status();
  }
  UserStat output{static_cast<uint64_t>(stat.value().type), stat.value().size};
  status = mem::VmmCopyOut(process->space, user_stat, &output, sizeof(output));
  if (IsErr(status)) {
    return status;
  }
  result = 0;
  return Status::kOk;
}

Status DoList(uint64_t user_path, uint64_t index, uint64_t user_entry,
              uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  if (index > 0xFFFFFFFFULL) {
    return Status::kInvalidArg;
  }
  char path[fs::kMaxPath];
  Status status = CopyUserPath(*process, user_path, path);
  if (IsErr(status)) {
    return status;
  }
  fs::DirEntry entry{};
  bool found = false;
  status = fs::FsList(path, static_cast<uint32_t>(index), entry, found);
  if (IsErr(status)) {
    return status;
  }
  if (!found) {
    result = 0;
    return Status::kOk;
  }
  UserDirEntry output{};
  mem::Copy(output.name, entry.name, sizeof(output.name));
  output.type = static_cast<uint64_t>(entry.type);
  output.size = entry.size;
  status = mem::VmmCopyOut(process->space, user_entry, &output, sizeof(output));
  if (IsErr(status)) {
    return status;
  }
  result = 1;
  return Status::kOk;
}

Status DoChdir(uint64_t user_path, uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  char path[fs::kMaxPath];
  Status status = CopyUserPath(*process, user_path, path);
  if (IsErr(status)) {
    return status;
  }
  const Result<fs::Stat> stat = fs::FsStat(path);
  if (stat.is_err()) {
    return stat.status();
  }
  if (stat.value().type != fs::NodeType::kDirectory) {
    return Status::kInvalidArg;
  }
  status = process::SetCwd(*process, path);
  if (IsErr(status)) {
    return status;
  }
  result = 0;
  return Status::kOk;
}

Status DoMkdir(uint64_t user_path, uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  char path[fs::kMaxPath];
  Status status = CopyUserPath(*process, user_path, path);
  if (IsErr(status)) {
    return status;
  }
  const Result<fs::Node*> created =
    fs::FsCreate(path, fs::NodeType::kDirectory);
  if (created.is_err()) {
    return created.status();
  }
  result = 0;
  return Status::kOk;
}

Status DoUnlink(uint64_t user_path, uint64_t& result) noexcept {
  return RemovePath(user_path, fs::NodeType::kFile, result);
}

Status DoRmdir(uint64_t user_path, uint64_t& result) noexcept {
  return RemovePath(user_path, fs::NodeType::kDirectory, result);
}

Status DoRename(uint64_t user_old_path, uint64_t user_new_path,
                uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  char old_path[fs::kMaxPath];
  char new_path[fs::kMaxPath];
  Status status = CopyUserPath(*process, user_old_path, old_path);
  if (IsErr(status)) {
    return status;
  }
  status = CopyUserPath(*process, user_new_path, new_path);
  if (IsErr(status)) {
    return status;
  }
  status = fs::FsRename(old_path, new_path);
  if (IsErr(status)) {
    return status;
  }
  result = 0;
  return Status::kOk;
}

Status DoMount(uint64_t user_type, uint64_t user_target,
               uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  char type[16];
  char target[fs::kMaxPath];
  Status status =
    CopyUserString(process->space, user_type, type, sizeof(type));
  if (IsErr(status)) {
    return status;
  }
  status = CopyUserPath(*process, user_target, target);
  if (IsErr(status)) {
    return status;
  }
  if (!mem::Same(type, "ramfs")) {
    return Status::kUnsupported;
  }
  const Result<fs::Stat> info = fs::FsStat(target);
  if (info.is_err()) {
    return info.status();
  }
  if (info.value().type != fs::NodeType::kDirectory) {
    return Status::kInvalidArg;
  }

  if (fs::FsIsMountRoot(target)) {
    status = fs::FsMountRef(target, nullptr);
  } else {
    const Result<fs::Node*> root = fs::ramfs::Create();
    if (root.is_err()) {
      return root.status();
    }
    status = fs::FsMountRef(target, root.value());
  }
  if (IsErr(status)) {
    return status;
  }

  status = process::RecordMount(*process, target);
  if (IsErr(status)) {
    (void)fs::FsUnmount(target);
    return status;
  }
  result = 0;
  return Status::kOk;
}

Status DoUmount(uint64_t user_target, uint64_t& result) noexcept {
  process::Process* process = CurrentProcess();
  if (process == nullptr) {
    return Status::kNotInit;
  }
  char target[fs::kMaxPath];
  Status status = CopyUserPath(*process, user_target, target);
  if (IsErr(status)) {
    return status;
  }
  if (!process::HasMount(*process, target)) {
    return Status::kNotFound;
  }
  status = fs::FsUnmount(target);
  if (IsErr(status)) {
    return status;
  }
  process::ForgetMount(*process, target);
  result = 0;
  return Status::kOk;
}

}  // namespace og::syscall
