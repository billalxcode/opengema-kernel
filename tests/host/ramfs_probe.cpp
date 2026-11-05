#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>

#include "og/core/status.h"
#include "og/fs/ramfs.h"
#include "og/mem/heap.h"
#include "memory.h"

namespace og::mem {

// Host probe allocator. The kernel ramfs only needs alloc and free.
Result<void*> HeapAlloc(uint64_t size) noexcept {
  void* pointer = std::malloc(static_cast<std::size_t>(size));
  if (pointer == nullptr) {
    return Result<void*>::Err(Status::kNoMem);
  }
  return Result<void*>::Ok(pointer);
}

Status HeapFree(void* pointer) noexcept {
  std::free(pointer);
  return Status::kOk;
}

}  // namespace og::mem

namespace {

bool ReadAll(og::fs::Node* node, char* buffer, std::size_t capacity,
             uint64_t& read) {
  if (node == nullptr || node->ops == nullptr || node->ops->read == nullptr) {
    return false;
  }
  return og::IsOk(node->ops->read(
    node, 0, std::span<uint8_t>(reinterpret_cast<uint8_t*>(buffer), capacity),
    read));
}

class RootGuard {
 public:
  explicit RootGuard(og::fs::Node* root) : root_(root) {}
  RootGuard(const RootGuard&) = delete;
  RootGuard& operator=(const RootGuard&) = delete;
  ~RootGuard() {
    if (root_ != nullptr && root_->ops != nullptr &&
        root_->ops->release != nullptr) {
      root_->ops->release(root_);
    }
  }

 private:
  og::fs::Node* root_;
};

}  // namespace

int main() {
  const auto root = og::fs::ramfs::Create();
  if (root.is_err()) {
    std::printf("ramfs probe: create failed\n");
    return 1;
  }
  RootGuard guard(root.value());
  const auto etc = og::fs::ramfs::CreateDir(root.value(), "etc");
  if (etc.is_err()) {
    std::printf("ramfs probe: etc failed\n");
    return 1;
  }
  const auto config = og::fs::ramfs::CreateFile(
    etc.value(), "gsl.conf", "a=1\n", 4);
  if (config.is_err()) {
    std::printf("ramfs probe: config failed\n");
    return 1;
  }

  og::fs::Node* found = nullptr;
  if (og::IsErr(etc.value()->ops->lookup(etc.value(), "gsl.conf", &found)) ||
      found != config.value()) {
    std::printf("ramfs probe: lookup failed\n");
    return 1;
  }
  char text[8]{};
  uint64_t read = 0;
  if (!ReadAll(found, text, sizeof(text), read) || read != 4U ||
      text[0] != 'a' || text[3] != '\n') {
    std::printf("ramfs probe: read failed\n");
    return 1;
  }

  const auto tmp = og::fs::ramfs::CreateDir(root.value(), "tmp");
  if (tmp.is_err()) {
    std::printf("ramfs probe: tmp failed\n");
    return 1;
  }
  const auto writer = og::fs::ramfs::CreateFile(tmp.value(), "writer", nullptr, 0);
  if (writer.is_err()) {
    std::printf("ramfs probe: writer failed\n");
    return 1;
  }
  const char payload[] = "write ok\n";
  uint64_t written = 0;
  if (og::IsErr(writer.value()->ops->write(
        writer.value(), 0,
        std::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(payload), sizeof(payload) - 1U),
        written)) ||
      written != (sizeof(payload) - 1U) ||
      writer.value()->size != (sizeof(payload) - 1U)) {
    std::printf("ramfs probe: write failed\n");
    return 1;
  }
  char readback[16]{};
  if (!ReadAll(writer.value(), readback, sizeof(readback), read) ||
      read != (sizeof(payload) - 1U) || readback[0] != 'w') {
    std::printf("ramfs probe: write readback failed\n");
    return 1;
  }

  og::fs::DirEntry entry{};
  bool has_entry = false;
  if (og::IsErr(tmp.value()->ops->iterate(tmp.value(), 0, entry, has_entry)) ||
      !has_entry || !og::mem::Same(entry.name, "writer")) {
    std::printf("ramfs probe: iterate failed\n");
    return 1;
  }

  if (og::fs::ramfs::CreateFile(nullptr, "bad", nullptr, 0).is_ok()) {
    std::printf("ramfs probe: null parent accepted\n");
    return 1;
  }

  std::printf("ramfs probe: OK\n");
  return 0;
}
