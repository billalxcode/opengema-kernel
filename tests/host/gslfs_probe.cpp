#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <span>
#include <vector>

#include "og/core/log.h"
#include "og/fs/gslfs.h"

namespace og {

// The GSLFS driver only logs at mount. The host probe does not need output.
void LogDec(LogLevel /*level*/, const char* /*label*/,
            uint64_t /*value*/) noexcept {}

}  // namespace og

namespace {

struct Disk {
  const std::vector<uint8_t>* data;
};

og::Status ReadDisk(void* context, uint64_t sector,
                    std::span<uint8_t> buffer) {
  auto* disk = static_cast<Disk*>(context);
  const uint64_t offset = sector * 512U;
  if (disk == nullptr || disk->data == nullptr ||
      offset + buffer.size() > disk->data->size()) {
    return og::Status::kIoError;
  }
  for (std::size_t index = 0; index < buffer.size(); ++index) {
    buffer[index] = (*disk->data)[offset + index];
  }
  return og::Status::kOk;
}

og::fs::Node* Resolve(og::fs::Node* root, const char* path) {
  og::fs::Node* current = root;
  const char* cursor = path;
  while (*cursor != '\0') {
    while (*cursor == '/') {
      ++cursor;
    }
    if (*cursor == '\0') {
      break;
    }
    char name[64];
    std::size_t length = 0;
    while (*cursor != '\0' && *cursor != '/') {
      if (length + 1U >= sizeof(name)) {
        return nullptr;
      }
      name[length] = *cursor;
      ++length;
      ++cursor;
    }
    name[length] = '\0';
    if (current->ops == nullptr || current->ops->lookup == nullptr) {
      return nullptr;
    }
    og::fs::Node* next = nullptr;
    if (og::IsErr(current->ops->lookup(current, name, &next))) {
      return nullptr;
    }
    current = next;
  }
  return current;
}

bool ReadText(og::fs::Node* node, char* buffer, std::size_t capacity,
              uint64_t& read) {
  if (node == nullptr || node->ops == nullptr || node->ops->read == nullptr) {
    return false;
  }
  return og::IsOk(node->ops->read(
    node, 0, std::span<uint8_t>(reinterpret_cast<uint8_t*>(buffer), capacity),
    read));
}

}  // namespace

int main(int argc, char** argv) {
  const bool expect_mount_fail =
    argc == 3 && std::strcmp(argv[2], "--expect-mount-fail") == 0;
  if (argc < 2 || argc > 3 || (argc == 3 && !expect_mount_fail)) {
    std::printf("usage: gslfs_probe IMAGE [--expect-mount-fail]\n");
    return 2;
  }
  std::ifstream file(argv[1], std::ios::binary);
  if (!file) {
    std::printf("gslfs probe: cannot open image\n");
    return 1;
  }
  std::vector<uint8_t> image{
    std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  if (image.empty() || (image.size() % 512U) != 0U) {
    std::printf("gslfs probe: bad image size\n");
    return 1;
  }

  Disk disk{&image};
  const og::fs::BlockDevice device{
    &ReadDisk, &disk, image.size() / 512U, 0};
  const auto root = og::fs::gslfs::Mount(device);
  if (expect_mount_fail) {
    if (root.is_err() && root.status() == og::Status::kInvalidArg) {
      std::printf("gslfs probe: mount rejected as expected\n");
      return 0;
    }
    std::printf("gslfs probe: expected an invalid tree\n");
    return 1;
  }
  if (root.is_err()) {
    std::printf("gslfs probe: mount failed: %u\n",
                static_cast<unsigned>(root.status()));
    return 1;
  }

  og::fs::Node* version = Resolve(root.value(), "/system/config/version");
  if (version == nullptr) {
    std::printf("gslfs probe: version not found\n");
    return 1;
  }
  char text[8]{};
  uint64_t read = 0;
  if (!ReadText(version, text, sizeof(text), read) || read != 4U ||
      text[0] != '0' || text[1] != '.' || text[2] != '1' ||
      text[3] != '\n') {
    std::printf("gslfs probe: version content mismatch\n");
    return 1;
  }

  og::fs::Node* hello = Resolve(root.value(), "/applications/hello/hello");
  if (hello == nullptr || hello->size == 0U) {
    std::printf("gslfs probe: hello not found\n");
    return 1;
  }

  if (Resolve(root.value(), "/missing") != nullptr) {
    std::printf("gslfs probe: missing path resolved\n");
    return 1;
  }
  if (og::fs::gslfs::Mount(device).status() != og::Status::kExists) {
    std::printf("gslfs probe: second mount was not rejected\n");
    return 1;
  }

  std::printf("gslfs probe: OK\n");
  return 0;
}
