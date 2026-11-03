// Mount table probe: shared references, deferred release, and pinned roots.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>

#include "memory.h"
#include "og/fs/ramfs.h"
#include "og/fs/vfs.h"
#include "og/mem/heap.h"

namespace {

int g_outstanding = 0;

}  // namespace

namespace og::mem {

Result<void*> HeapAlloc(uint64_t size) noexcept {
  void* pointer = std::malloc(static_cast<std::size_t>(size));
  if (pointer == nullptr) {
    return Result<void*>::Err(Status::kNoMem);
  }
  ++g_outstanding;
  return Result<void*>::Ok(pointer);
}

Status HeapFree(void* pointer) noexcept {
  if (pointer != nullptr) {
    std::free(pointer);
    --g_outstanding;
  }
  return Status::kOk;
}

}  // namespace og::mem

namespace {

int Fail(const char* message) {
  std::printf("vfs mount probe: %s\n", message);
  return 1;
}

}  // namespace

int main() {
  const auto root = og::fs::ramfs::Create();
  const auto extra = og::fs::ramfs::Create();
  if (root.is_err() || extra.is_err()) {
    return Fail("ramfs create");
  }
  if (!og::IsOk(og::fs::FsInit()) ||
      !og::IsOk(og::fs::FsMount("/", root.value()))) {
    return Fail("root mount");
  }
  if (og::fs::FsMount("/", root.value()) != og::Status::kExists) {
    return Fail("duplicate root mount accepted");
  }
  if (og::fs::FsUnmount("/") != og::Status::kBusy) {
    return Fail("pinned root unmounted");
  }
  if (!og::fs::FsIsMountRoot("/")) {
    return Fail("root not reported as mount");
  }

  if (og::fs::FsCreate("/mnt", og::fs::NodeType::kDirectory).is_err()) {
    return Fail("mkdir mnt");
  }
  if (!og::IsOk(og::fs::FsMountRef("/mnt", extra.value()))) {
    return Fail("shared mount");
  }
  if (!og::fs::FsIsMountRoot("/mnt")) {
    return Fail("mnt not reported as mount");
  }
  if (og::fs::FsCreate("/mnt/hello", og::fs::NodeType::kFile).is_err()) {
    return Fail("create under mount");
  }

  og::fs::FileTable table{};
  if (!og::IsOk(og::fs::FileTableInit(table))) {
    return Fail("file table");
  }
  const auto descriptor =
    og::fs::FsOpen(table, "/mnt/hello", og::fs::kOpenWrite);
  if (descriptor.is_err()) {
    return Fail("open under mount");
  }
  constexpr char kText[] = "hi";
  uint64_t written = 0;
  if (!og::IsOk(og::fs::FsWrite(
        table, descriptor.value(),
        std::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(kText), sizeof(kText) - 1U),
        written)) ||
      written != (sizeof(kText) - 1U)) {
    return Fail("write under mount");
  }

  if (!og::IsOk(og::fs::FsMountRef("/mnt", nullptr))) {
    return Fail("add mount reference");
  }
  if (!og::IsOk(og::fs::FsUnmount("/mnt"))) {
    return Fail("drop first reference");
  }
  if (og::fs::FsStat("/mnt/hello").is_err()) {
    return Fail("mount disappeared with a live reference");
  }
  if (!og::IsOk(og::fs::FsUnmount("/mnt"))) {
    return Fail("drop final reference");
  }
  if (og::fs::FsIsMountRoot("/mnt")) {
    return Fail("mnt still reported as mount");
  }
  if (og::fs::FsStat("/mnt/hello").is_ok()) {
    return Fail("mounted file still visible");
  }
  if (og::fs::FsStat("/mnt").is_err()) {
    return Fail("underlying directory hidden after unmount");
  }

  // The open descriptor keeps the unmounted root alive.
  uint64_t written_after = 0;
  if (!og::IsOk(og::fs::FsWrite(
        table, descriptor.value(),
        std::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(kText), sizeof(kText) - 1U),
        written_after)) ||
      written_after != (sizeof(kText) - 1U)) {
    return Fail("write after unmount");
  }
  if (!og::IsOk(og::fs::FsClose(table, descriptor.value()))) {
    return Fail("close after unmount");
  }
  if (g_outstanding != 2) {
    return Fail("unmount did not release the root");
  }

  if (og::fs::FsUnmount("/mnt") != og::Status::kNotFound) {
    return Fail("second unmount accepted");
  }
  if (og::fs::FsMountRef("/missing", nullptr) != og::Status::kInvalidArg) {
    return Fail("reference to missing mount accepted");
  }

  const auto extra2 = og::fs::ramfs::Create();
  if (extra2.is_err()) {
    return Fail("second ramfs create");
  }
  if (!og::IsOk(og::fs::FsMountRef("/mnt", extra2.value()))) {
    return Fail("remount");
  }
  if (og::fs::FsMountRef("/mnt", extra2.value()) != og::Status::kExists) {
    return Fail("duplicate root accepted");
  }
  if (!og::IsOk(og::fs::FsUnmount("/mnt"))) {
    return Fail("remount unmount");
  }
  if (g_outstanding != 2) {
    return Fail("second root was not released");
  }

  std::printf("vfs mount probe: OK\n");
  return 0;
}
