#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>

#include "og/fs/devfs.h"

namespace {

char g_console_text[256];
std::size_t g_console_length = 0;

bool ReadAll(og::fs::Node* node, uint8_t* buffer, std::size_t capacity,
             uint64_t& read) {
  if (node == nullptr || node->ops == nullptr || node->ops->read == nullptr) {
    return false;
  }
  return og::IsOk(node->ops->read(
    node, 0, std::span<uint8_t>(buffer, capacity), read));
}

}  // namespace

namespace og::io {

// devfs only needs this hook; the probe captures the text instead of
// touching a real console.
Status ConsoleWriteTextActive(const char* text) noexcept {
  if (text == nullptr) {
    return Status::kInvalidArg;
  }
  std::size_t length = 0;
  while (text[length] != '\0') {
    ++length;
  }
  if ((g_console_length + length) >= sizeof(g_console_text)) {
    return Status::kNoMem;
  }
  for (std::size_t index = 0; index < length; ++index) {
    g_console_text[g_console_length + index] = text[index];
  }
  g_console_length += length;
  g_console_text[g_console_length] = '\0';
  return Status::kOk;
}

}  // namespace og::io

int main() {
  const auto root = og::fs::devfs::Create();
  if (root.is_err()) {
    std::printf("devfs probe: create failed\n");
    return 1;
  }
  const auto again = og::fs::devfs::Create();
  if (again.is_err() || again.value() != root.value()) {
    std::printf("devfs probe: create is not idempotent\n");
    return 1;
  }
  if (root.value()->type != og::fs::NodeType::kDirectory) {
    std::printf("devfs probe: root is not a directory\n");
    return 1;
  }

  og::fs::Node* console = nullptr;
  og::fs::Node* null = nullptr;
  og::fs::Node* zero = nullptr;
  if (og::IsErr(root.value()->ops->lookup(root.value(), "console", &console)) ||
      og::IsErr(root.value()->ops->lookup(root.value(), "null", &null)) ||
      og::IsErr(root.value()->ops->lookup(root.value(), "zero", &zero))) {
    std::printf("devfs probe: device lookup failed\n");
    return 1;
  }
  if (console == nullptr || null == nullptr || zero == nullptr ||
      console->type != og::fs::NodeType::kDevice ||
      null->type != og::fs::NodeType::kDevice ||
      zero->type != og::fs::NodeType::kDevice) {
    std::printf("devfs probe: device node mismatch\n");
    return 1;
  }

  og::fs::Node* missing = console;
  if (root.value()->ops->lookup(root.value(), "missing", &missing) !=
        og::Status::kNotFound ||
      missing != nullptr) {
    std::printf("devfs probe: missing lookup failed\n");
    return 1;
  }
  if (root.value()->ops->lookup(root.value(), nullptr, &missing) !=
        og::Status::kInvalidArg ||
      root.value()->ops->lookup(root.value(), "console", nullptr) !=
        og::Status::kInvalidArg) {
    std::printf("devfs probe: invalid lookup failed\n");
    return 1;
  }

  const char* names[] = {"console", "null", "zero"};
  for (uint32_t index = 0; index < 3U; ++index) {
    og::fs::DirEntry entry{};
    bool found = false;
    if (og::IsErr(root.value()->ops->iterate(root.value(), index, entry,
                                             found)) ||
        !found || entry.type != og::fs::NodeType::kDevice) {
      std::printf("devfs probe: iterate %u failed\n", index);
      return 1;
    }
    bool matches = true;
    for (std::size_t offset = 0; names[index][offset] != '\0'; ++offset) {
      if (entry.name[offset] != names[index][offset]) {
        matches = false;
        break;
      }
    }
    if (!matches) {
      std::printf("devfs probe: iterate %u name mismatch\n", index);
      return 1;
    }
  }
  og::fs::DirEntry entry{};
  bool found = true;
  if (og::IsErr(root.value()->ops->iterate(root.value(), 3, entry, found)) ||
      found) {
    std::printf("devfs probe: iterate past end failed\n");
    return 1;
  }

  const char text[] = "abc";
  uint64_t written = 0;
  if (og::IsErr(console->ops->write(
        console, 0,
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(text), 3),
        written)) ||
      written != 3U || g_console_length != 3U || g_console_text[0] != 'a' ||
      g_console_text[2] != 'c') {
    std::printf("devfs probe: console write failed\n");
    return 1;
  }
  if (console->ops->write(
        console, 0, std::span<const uint8_t>(), written) !=
      og::Status::kInvalidArg) {
    std::printf("devfs probe: empty console write was accepted\n");
    return 1;
  }

  uint8_t buffer[4] = {0xFFU, 0xFFU, 0xFFU, 0xFFU};
  uint64_t read = 0;
  if (!ReadAll(null, buffer, sizeof(buffer), read) || read != 0U ||
      buffer[0] != 0xFFU) {
    std::printf("devfs probe: null read failed\n");
    return 1;
  }
  written = 0;
  if (og::IsErr(null->ops->write(
        null, 0,
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(text), 3),
        written)) ||
      written != 3U) {
    std::printf("devfs probe: null write failed\n");
    return 1;
  }

  for (std::size_t index = 0; index < sizeof(buffer); ++index) {
    buffer[index] = 0xFFU;
  }
  read = 0;
  if (!ReadAll(zero, buffer, sizeof(buffer), read) ||
      read != sizeof(buffer)) {
    std::printf("devfs probe: zero read failed\n");
    return 1;
  }
  for (std::size_t index = 0; index < sizeof(buffer); ++index) {
    if (buffer[index] != 0U) {
      std::printf("devfs probe: zero read was not zeroed\n");
      return 1;
    }
  }
  written = 0;
  if (og::IsErr(zero->ops->write(
        zero, 0,
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(text), 3),
        written)) ||
      written != 3U) {
    std::printf("devfs probe: zero write failed\n");
    return 1;
  }

  std::printf("devfs probe: OK\n");
  return 0;
}
