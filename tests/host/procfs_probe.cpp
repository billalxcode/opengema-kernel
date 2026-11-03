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

namespace og::log_ring {

uint32_t Read(uint64_t offset, char* buffer, uint32_t capacity) noexcept {
  constexpr char kText[] = "dmesg line\n";
  constexpr uint32_t kSize = sizeof(kText) - 1U;
  if (buffer == nullptr || capacity == 0U || offset >= kSize) {
    return 0;
  }
  uint32_t count = kSize - static_cast<uint32_t>(offset);
  if (count > capacity) {
    count = capacity;
  }
  for (uint32_t index = 0; index < count; ++index) {
    buffer[index] = kText[offset + index];
  }
  return count;
}

uint32_t Size() noexcept {
  return 11U;
}

void Append(const char* /*text*/, uint32_t /*length*/) noexcept {}

}  // namespace og::log_ring

namespace {

uint32_t ThreadCount() noexcept {
  return 7U;
}

og::Status ProcessText(char* buffer, std::size_t capacity,
                       std::size_t& length) noexcept {
  constexpr char kText[] = "pid ppid state name\n7 1 ready test\n";
  constexpr std::size_t kSize = sizeof(kText) - 1U;
  if (buffer == nullptr || capacity < kSize) {
    return og::Status::kNoMem;
  }
  for (std::size_t index = 0; index < kSize; ++index) {
    buffer[index] = kText[index];
  }
  length = kSize;
  return og::Status::kOk;
}

og::Status BlockText(char* buffer, std::size_t capacity,
                     std::size_t& length) noexcept {
  constexpr char kText[] = "virtio-blk0 100\nvirtio-blk1 200\n";
  constexpr std::size_t kSize = sizeof(kText) - 1U;
  if (buffer == nullptr || capacity < kSize) {
    return og::Status::kNoMem;
  }
  for (std::size_t index = 0; index < kSize; ++index) {
    buffer[index] = kText[index];
  }
  length = kSize;
  return og::Status::kOk;
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
  const auto root = og::fs::procfs::Create(&ThreadCount, &ProcessText);
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
  char text[64]{};
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

  og::fs::Node* processes = nullptr;
  if (og::IsErr(root.value()->ops->lookup(root.value(), "processes",
                                          &processes)) ||
      processes == nullptr) {
    std::printf("procfs probe: processes lookup failed\n");
    return 1;
  }
  if (!ReadAll(processes, 0, text, sizeof(text), read) || read != 35U ||
      text[0] != 'p' || text[4] != 'p' || text[19] != '\n' ||
      text[20] != '7' || text[34] != '\n') {
    std::printf("procfs probe: processes content mismatch\n");
    return 1;
  }

  og::fs::Node* dmesg = nullptr;
  if (og::IsErr(root.value()->ops->lookup(root.value(), "dmesg", &dmesg)) ||
      dmesg == nullptr) {
    std::printf("procfs probe: dmesg lookup failed\n");
    return 1;
  }
  if (!ReadAll(dmesg, 0, text, sizeof(text), read) || read != 11U ||
      text[0] != 'd' || text[10] != '\n') {
    std::printf("procfs probe: dmesg content mismatch\n");
    return 1;
  }

  og::fs::Node* block = nullptr;
  if (og::IsErr(root.value()->ops->lookup(root.value(), "block", &block)) ||
      block == nullptr) {
    std::printf("procfs probe: block lookup failed\n");
    return 1;
  }
  if (!ReadAll(block, 0, text, sizeof(text), read) || read != 17U ||
      text[0] != 'n' || text[16] != '\n') {
    std::printf("procfs probe: block content mismatch\n");
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
      !found || entry.name[0] != 'p') {
    std::printf("procfs probe: iterate fourth entry failed\n");
    return 1;
  }
  if (og::IsErr(root.value()->ops->iterate(root.value(), 4, entry, found)) ||
      !found || entry.name[0] != 'd') {
    std::printf("procfs probe: iterate fifth entry failed\n");
    return 1;
  }
  if (og::IsErr(root.value()->ops->iterate(root.value(), 5, entry, found)) ||
      !found || entry.name[0] != 'b') {
    std::printf("procfs probe: iterate sixth entry failed\n");
    return 1;
  }
  if (og::IsErr(root.value()->ops->iterate(root.value(), 6, entry, found)) ||
      found) {
    std::printf("procfs probe: iterate past end failed\n");
    return 1;
  }

  const auto custom =
    og::fs::procfs::Create(&ThreadCount, &ProcessText, &BlockText);
  if (custom.is_err()) {
    std::printf("procfs probe: custom create failed\n");
    return 1;
  }
  og::fs::Node* custom_block = nullptr;
  if (og::IsErr(custom.value()->ops->lookup(custom.value(), "block",
                                            &custom_block)) ||
      custom_block == nullptr) {
    std::printf("procfs probe: custom block lookup failed\n");
    return 1;
  }
  constexpr std::size_t kBlockSize =
    sizeof("virtio-blk0 100\nvirtio-blk1 200\n") - 1U;
  if (!ReadAll(custom_block, 0, text, sizeof(text), read) ||
      read != kBlockSize || text[0] != 'v' ||
      text[kBlockSize - 1U] != '\n') {
    std::printf("procfs probe: custom block content mismatch\n");
    return 1;
  }

  std::printf("procfs probe: OK\n");
  return 0;
}
