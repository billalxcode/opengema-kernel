#include <cstdint>
#include <cstdio>
#include <span>

#include "og/fs/procfs.h"

namespace og::time {

// The probe controls uptime so the generated text is stable.
uint64_t TimeNowMs() noexcept {
  return 1234U;
}

}  // namespace og::time

namespace {

uint32_t ThreadCount() noexcept {
  return 7U;
}

bool ReadAll(og::fs::Node* node, uint64_t offset, char* buffer,
             std::size_t capacity, uint64_t& read) {
  if (node == nullptr || node->ops == nullptr || node->ops->read == nullptr) {
    return false;
  }
  return og::IsOk(node->ops->read(
    node, offset,
    std::span<uint8_t>(reinterpret_cast<uint8_t*>(buffer), capacity), read));
}

}  // namespace

int main() {
  const auto root = og::fs::procfs::Create(&ThreadCount);
  if (root.is_err()) {
    std::printf("procfs probe: create failed\n");
    return 1;
  }

  og::fs::Node* uptime = nullptr;
  if (og::IsErr(root.value()->ops->lookup(root.value(), "uptime", &uptime)) ||
      uptime == nullptr) {
    std::printf("procfs probe: uptime lookup failed\n");
    return 1;
  }
  char text[32]{};
  uint64_t read = 0;
  if (!ReadAll(uptime, 0, text, sizeof(text), read) || read != 8U ||
      text[0] != '1' || text[3] != '4' || text[4] != ' ' || text[5] != 'm' ||
      text[6] != 's' || text[7] != '\n') {
    std::printf("procfs probe: uptime content mismatch\n");
    return 1;
  }

  // Reads start at the requested offset and stop at the end of the text.
  if (!ReadAll(uptime, 5, text, sizeof(text), read) || read != 3U ||
      text[0] != 'm' || text[1] != 's' || text[2] != '\n') {
    std::printf("procfs probe: uptime offset failed\n");
    return 1;
  }
  if (!ReadAll(uptime, 8, text, sizeof(text), read) || read != 0U) {
    std::printf("procfs probe: uptime eof failed\n");
    return 1;
  }

  og::fs::Node* threads = nullptr;
  if (og::IsErr(root.value()->ops->lookup(root.value(), "threads", &threads)) ||
      threads == nullptr) {
    std::printf("procfs probe: threads lookup failed\n");
    return 1;
  }
  if (!ReadAll(threads, 0, text, sizeof(text), read) || read != 2U ||
      text[0] != '7' || text[1] != '\n') {
    std::printf("procfs probe: threads content mismatch\n");
    return 1;
  }

  og::fs::Node* acpi = nullptr;
  if (og::IsErr(root.value()->ops->lookup(root.value(), "acpi", &acpi)) ||
      acpi == nullptr) {
    std::printf("procfs probe: acpi lookup failed\n");
    return 1;
  }
  if (!ReadAll(acpi, 0, text, sizeof(text), read) || read != 17U ||
      text[0] != 'A' || text[4] != ' ' || text[5] != 'u' || text[15] != 'e' ||
      text[16] != '\n') {
    std::printf("procfs probe: acpi content mismatch\n");
    return 1;
  }

  if (root.value()->ops->lookup(root.value(), "missing", &threads) !=
      og::Status::kNotFound) {
    std::printf("procfs probe: missing lookup was not rejected\n");
    return 1;
  }

  og::fs::DirEntry entry{};
  bool found = false;
  if (og::IsErr(root.value()->ops->iterate(root.value(), 0, entry, found)) ||
      !found || entry.name[0] != 'u' || entry.type != og::fs::NodeType::kFile) {
    std::printf("procfs probe: iterate first entry failed\n");
    return 1;
  }
  if (og::IsErr(root.value()->ops->iterate(root.value(), 1, entry, found)) ||
      !found || entry.name[0] != 't') {
    std::printf("procfs probe: iterate second entry failed\n");
    return 1;
  }
  if (og::IsErr(root.value()->ops->iterate(root.value(), 2, entry, found)) ||
      !found || entry.name[0] != 'a') {
    std::printf("procfs probe: iterate third entry failed\n");
    return 1;
  }
  if (og::IsErr(root.value()->ops->iterate(root.value(), 3, entry, found)) ||
      found) {
    std::printf("procfs probe: iterate past end failed\n");
    return 1;
  }

  std::printf("procfs probe: OK\n");
  return 0;
}
