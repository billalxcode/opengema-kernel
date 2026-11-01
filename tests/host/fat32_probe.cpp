#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <vector>

#include "og/fs/fat32.h"
#include "og/mem/heap.h"
#include "memory.h"

namespace og::mem {

// The FAT32 driver only allocates its cluster buffer on mount.
Result<void*> HeapAlloc(uint64_t size) noexcept {
  void* pointer = std::malloc(static_cast<std::size_t>(size));
  if (pointer == nullptr) {
    return Result<void*>::Err(Status::kNoMem);
  }
  return Result<void*>::Ok(pointer);
}

}  // namespace og::mem

namespace {

constexpr uint32_t kSectorSize = 512;
constexpr uint32_t kSectorCount = 64;

void PutLe16(uint8_t* data, uint16_t value) {
  data[0] = static_cast<uint8_t>(value);
  data[1] = static_cast<uint8_t>(value >> 8U);
}

void PutLe32(uint8_t* data, uint32_t value) {
  for (uint32_t index = 0; index < 4U; ++index) {
    data[index] = static_cast<uint8_t>(value >> (index * 8U));
  }
}

struct Disk {
  const std::vector<uint8_t>* data;
};

og::Status ReadDisk(void* context, uint64_t sector,
                    std::span<uint8_t> buffer) {
  auto* disk = static_cast<Disk*>(context);
  const uint64_t offset = sector * kSectorSize;
  if (disk == nullptr || disk->data == nullptr ||
      offset + buffer.size() > disk->data->size()) {
    return og::Status::kIoError;
  }
  for (std::size_t index = 0; index < buffer.size(); ++index) {
    buffer[index] = (*disk->data)[offset + index];
  }
  return og::Status::kOk;
}

void BuildImage(std::vector<uint8_t>& image) {
  image.assign(kSectorCount * kSectorSize, 0);
  uint8_t* boot = image.data();
  PutLe16(boot + 11U, kSectorSize);
  boot[13U] = 1U;  // sectors per cluster
  PutLe16(boot + 14U, 1U);
  boot[16U] = 1U;  // one FAT
  PutLe32(boot + 32U, kSectorCount);
  PutLe32(boot + 36U, 1U);  // FAT size in sectors
  PutLe32(boot + 44U, 2U);  // root cluster
  boot[510U] = 0x55U;
  boot[511U] = 0xAAU;

  // FAT at sector 1. Reserved entries plus one end marker per cluster.
  uint8_t* fat = image.data() + kSectorSize;
  PutLe32(fat + 0U, 0x0FFFFFF8U);
  PutLe32(fat + 4U, 0x0FFFFFFFU);
  for (uint32_t cluster = 2U; cluster <= 5U; ++cluster) {
    PutLe32(fat + (cluster * 4U), 0x0FFFFFFFU);
  }

  // Cluster 2: root directory with a file and a subdirectory.
  uint8_t* root = image.data() + (2U * kSectorSize);
  og::mem::Copy(root, "HELLO   TXT", 11);
  root[11U] = 0x20U;
  PutLe16(root + 26U, 3U);
  PutLe32(root + 28U, 12U);

  uint8_t* sub = root + 32U;
  og::mem::Copy(sub, "SUB        ", 11);
  sub[11U] = 0x10U;
  PutLe16(sub + 26U, 4U);

  // Cluster 3: file data. Cluster 4: a subdirectory. Cluster 5: its file.
  og::mem::Copy(image.data() + (3U * kSectorSize), "hello world\n", 12);

  uint8_t* subdir = image.data() + (4U * kSectorSize);
  og::mem::Copy(subdir, "INNER   TXT", 11);
  subdir[11U] = 0x20U;
  PutLe16(subdir + 26U, 5U);
  PutLe32(subdir + 28U, 5U);

  og::mem::Copy(image.data() + (5U * kSectorSize), "inner", 5);
}

bool ReadAll(og::fs::Node* node, uint64_t offset, char* buffer,
             std::size_t capacity, uint64_t& read) {
  if (node == nullptr || node->ops == nullptr || node->ops->read == nullptr) {
    return false;
  }
  og::mem::Zero(buffer, capacity);
  return og::IsOk(node->ops->read(
    node, offset,
    std::span<uint8_t>(reinterpret_cast<uint8_t*>(buffer), capacity), read));
}

}  // namespace

int main() {
  std::vector<uint8_t> image;
  BuildImage(image);
  Disk disk{&image};
  const og::fs::BlockDevice device{&ReadDisk, &disk, kSectorCount, 0};

  og::fs::Node* root = nullptr;
  if (og::IsErr(og::fs::fat32::Mount(device, &root)) || root == nullptr) {
    std::printf("fat32 probe: mount failed\n");
    return 1;
  }
  if (root->type != og::fs::NodeType::kDirectory ||
      !og::mem::Same(root->name, "/")) {
    std::printf("fat32 probe: root mismatch\n");
    return 1;
  }

  og::fs::Node* hello = nullptr;
  if (og::IsErr(root->ops->lookup(root, "hello.txt", &hello)) ||
      hello == nullptr || hello->type != og::fs::NodeType::kFile ||
      hello->size != 12U) {
    std::printf("fat32 probe: hello lookup failed\n");
    return 1;
  }
  char text[16]{};
  uint64_t read = 0;
  if (!ReadAll(hello, 0, text, sizeof(text), read) || read != 12U ||
      !og::mem::Same(text, "hello world\n")) {
    std::printf("fat32 probe: hello read failed\n");
    return 1;
  }

  // Lookup is case-insensitive and supports offsets within a file.
  og::fs::Node* upper = nullptr;
  if (og::IsErr(root->ops->lookup(root, "HELLO.TXT", &upper)) ||
      upper == nullptr || !og::mem::Same(upper->name, "HELLO.TXT") ||
      upper->type != og::fs::NodeType::kFile || upper->size != 12U) {
    std::printf("fat32 probe: case-insensitive lookup failed\n");
    return 1;
  }
  if (!ReadAll(hello, 6, text, sizeof(text), read) || read != 6U ||
      !og::mem::Same(text, "world\n")) {
    std::printf("fat32 probe: offset read failed\n");
    return 1;
  }
  if (!ReadAll(hello, 12, text, sizeof(text), read) || read != 0U) {
    std::printf("fat32 probe: eof read failed\n");
    return 1;
  }

  og::fs::Node* missing = hello;
  if (root->ops->lookup(root, "missing.txt", &missing) !=
        og::Status::kNotFound ||
      missing != nullptr) {
    std::printf("fat32 probe: missing lookup failed\n");
    return 1;
  }
  if (root->ops->lookup(root, nullptr, &missing) != og::Status::kInvalidArg ||
      root->ops->lookup(root, "hello.txt", nullptr) != og::Status::kInvalidArg) {
    std::printf("fat32 probe: invalid lookup failed\n");
    return 1;
  }

  og::fs::Node* subdir = nullptr;
  if (og::IsErr(root->ops->lookup(root, "sub", &subdir)) ||
      subdir == nullptr || subdir->type != og::fs::NodeType::kDirectory) {
    std::printf("fat32 probe: subdir lookup failed\n");
    return 1;
  }
  og::fs::Node* inner = nullptr;
  if (og::IsErr(subdir->ops->lookup(subdir, "inner.txt", &inner)) ||
      inner == nullptr || inner->size != 5U) {
    std::printf("fat32 probe: nested lookup failed\n");
    return 1;
  }
  if (!ReadAll(inner, 0, text, sizeof(text), read) || read != 5U ||
      !og::mem::Same(text, "inner")) {
    std::printf("fat32 probe: nested read failed\n");
    return 1;
  }

  og::fs::DirEntry entry{};
  bool found = false;
  if (og::IsErr(root->ops->iterate(root, 0, entry, found)) || !found ||
      !og::mem::Same(entry.name, "HELLO.TXT") ||
      entry.type != og::fs::NodeType::kFile || entry.size != 12U) {
    std::printf("fat32 probe: iterate file failed\n");
    return 1;
  }
  if (og::IsErr(root->ops->iterate(root, 1, entry, found)) || !found ||
      !og::mem::Same(entry.name, "SUB") ||
      entry.type != og::fs::NodeType::kDirectory) {
    std::printf("fat32 probe: iterate directory failed\n");
    return 1;
  }
  if (og::IsErr(root->ops->iterate(root, 2, entry, found)) || found) {
    std::printf("fat32 probe: iterate past end failed\n");
    return 1;
  }

  if (og::fs::fat32::Mount(device, nullptr) != og::Status::kInvalidArg) {
    std::printf("fat32 probe: null root was accepted\n");
    return 1;
  }
  const og::fs::BlockDevice no_read{nullptr, nullptr, 0, 0};
  if (og::fs::fat32::Mount(no_read, &root) != og::Status::kInvalidArg) {
    std::printf("fat32 probe: null reader was accepted\n");
    return 1;
  }

  std::vector<uint8_t> broken = image;
  broken[510U] = 0U;
  broken[511U] = 0U;
  Disk broken_disk{&broken};
  const og::fs::BlockDevice broken_device{&ReadDisk, &broken_disk, kSectorCount,
                                          0};
  if (og::fs::fat32::Mount(broken_device, &root) != og::Status::kNoDevice) {
    std::printf("fat32 probe: bad signature was accepted\n");
    return 1;
  }

  std::vector<uint8_t> unsupported = image;
  PutLe16(unsupported.data() + 11U, 256U);
  Disk unsupported_disk{&unsupported};
  const og::fs::BlockDevice unsupported_device{
    &ReadDisk, &unsupported_disk, kSectorCount, 0};
  if (og::fs::fat32::Mount(unsupported_device, &root) !=
      og::Status::kUnsupported) {
    std::printf("fat32 probe: bad sector size was accepted\n");
    return 1;
  }

  std::vector<uint8_t> bad_cluster = image;
  bad_cluster[13U] = 3U;
  Disk bad_cluster_disk{&bad_cluster};
  const og::fs::BlockDevice bad_cluster_device{&ReadDisk, &bad_cluster_disk,
                                               kSectorCount, 0};
  if (og::fs::fat32::Mount(bad_cluster_device, &root) !=
      og::Status::kInvalidArg) {
    std::printf("fat32 probe: bad cluster size was accepted\n");
    return 1;
  }

  std::vector<uint8_t> bad_layout = image;
  PutLe16(bad_layout.data() + 14U, 64U);
  Disk bad_layout_disk{&bad_layout};
  const og::fs::BlockDevice bad_layout_device{&ReadDisk, &bad_layout_disk,
                                              kSectorCount, 0};
  if (og::fs::fat32::Mount(bad_layout_device, &root) !=
      og::Status::kInvalidArg) {
    std::printf("fat32 probe: bad data start was accepted\n");
    return 1;
  }

  std::printf("fat32 probe: OK\n");
  return 0;
}
