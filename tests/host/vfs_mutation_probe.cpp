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
  std::printf("vfs mutation probe: %s\n", message);
  return 1;
}

}  // namespace

int main() {
  const auto root = og::fs::ramfs::Create();
  if (root.is_err()) {
    return Fail("ramfs create");
  }
  if (!og::IsOk(og::fs::FsInit()) ||
      !og::IsOk(og::fs::FsMount("/", root.value()))) {
    return Fail("mount");
  }

  const auto directory =
    og::fs::FsCreate("/dir", og::fs::NodeType::kDirectory);
  if (directory.is_err()) {
    return Fail("mkdir");
  }
  const auto file = og::fs::FsCreate("/dir/file", og::fs::NodeType::kFile);
  if (file.is_err()) {
    return Fail("create file");
  }
  if (og::fs::FsCreate("/dir/file", og::fs::NodeType::kFile).is_ok()) {
    return Fail("duplicate create accepted");
  }

  og::fs::FileTable table{};
  if (!og::IsOk(og::fs::FileTableInit(table))) {
    return Fail("file table");
  }
  const auto descriptor =
    og::fs::FsOpen(table, "/dir/file", og::fs::kOpenWrite);
  if (descriptor.is_err()) {
    return Fail("open file");
  }
  const char payload[] = "hi";
  uint64_t written = 0;
  if (!og::IsOk(og::fs::FsWrite(
        table, descriptor.value(),
        std::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(payload), 2),
        written)) ||
      written != 2U) {
    return Fail("write file");
  }
  char readback[4]{};
  uint64_t read = 0;
  if (!og::IsOk(og::fs::FsSeek(table, descriptor.value(), 0)) ||
      !og::IsOk(og::fs::FsRead(
        table, descriptor.value(),
        std::span<uint8_t>(reinterpret_cast<uint8_t*>(readback), 4),
        read)) ||
      read != 2U || readback[0] != 'h' || readback[1] != 'i') {
    return Fail("read file");
  }
  if (!og::IsOk(og::fs::FsClose(table, descriptor.value()))) {
    return Fail("close file");
  }

  if (!og::IsOk(og::fs::FsChmod("/dir/file", 0644U))) {
    return Fail("chmod");
  }
  const auto mode = og::fs::FsStat("/dir/file");
  if (mode.is_err() || mode.value().mode != 0644U) {
    return Fail("chmod result");
  }
  if (og::fs::FsChmod("/dir/file", 01000U) != og::Status::kInvalidArg) {
    return Fail("invalid chmod accepted");
  }
  if (!og::IsOk(og::fs::FsChown("/dir/file", 7U, 8U))) {
    return Fail("chown");
  }
  const auto owner = og::fs::FsStat("/dir/file");
  if (owner.is_err() || owner.value().uid != 7U ||
      owner.value().gid != 8U) {
    return Fail("chown result");
  }

  table.uid = 1U;
  table.gid = 1U;
  if (og::fs::FsOpen(table, "/dir/file", og::fs::kOpenWrite).status() !=
      og::Status::kDenied) {
    return Fail("non-owner write was allowed");
  }
  const auto reader = og::fs::FsOpen(table, "/dir/file");
  if (reader.is_err() ||
      !og::IsOk(og::fs::FsClose(table, reader.value()))) {
    return Fail("read-only open failed");
  }

  if (!og::IsOk(og::fs::FsRename("/dir/file", "/dir/renamed"))) {
    return Fail("rename");
  }
  if (og::fs::FsStat("/dir/file").is_ok() ||
      og::fs::FsStat("/dir/renamed").is_err()) {
    return Fail("rename result");
  }

  const auto sub = og::fs::FsCreate("/dir/sub", og::fs::NodeType::kDirectory);
  if (sub.is_err()) {
    return Fail("mkdir sub");
  }
  if (og::fs::FsRemove("/dir", og::fs::NodeType::kDirectory) !=
      og::Status::kBusy) {
    return Fail("non-empty directory removed");
  }
  if (!og::IsOk(og::fs::FsRemove("/dir/renamed", og::fs::NodeType::kFile))) {
    return Fail("unlink");
  }
  if (!og::IsOk(og::fs::FsRename("/dir/sub", "/dir/sub2"))) {
    return Fail("rename directory");
  }
  if (!og::IsOk(og::fs::FsRemove("/dir/sub2", og::fs::NodeType::kDirectory))) {
    return Fail("rmdir");
  }
  if (!og::IsOk(og::fs::FsRemove("/dir", og::fs::NodeType::kDirectory))) {
    return Fail("remove directory");
  }
  if (og::fs::FsRemove("/", og::fs::NodeType::kDirectory) !=
      og::Status::kBusy) {
    return Fail("mount root removed");
  }
  if (og::fs::FsCreate("/", og::fs::NodeType::kDirectory).is_ok()) {
    return Fail("mount root recreated");
  }
  if (g_outstanding != 1) {
    return Fail("node allocation leak");
  }

  std::printf("vfs mutation probe: OK\n");
  return 0;
}
