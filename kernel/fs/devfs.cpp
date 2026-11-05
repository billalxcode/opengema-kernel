#include "og/fs/devfs.h"

#include "memory.h"
#include "og/io/console.h"

namespace og::fs::devfs {

namespace {

Node g_root{};
Node g_console{};
Node g_null{};
Node g_zero{};

constexpr uint32_t kDeviceCount = 3;
// Single source for lookup and iteration order.
Node* const g_devices[kDeviceCount] = {&g_console, &g_null, &g_zero};

bool g_created = false;

Status ConsoleWrite(Node* /*node*/, uint64_t /*offset*/, std::span<const uint8_t> buffer,
                    uint64_t& out_written) noexcept {
  out_written = 0;
  if (buffer.empty()) {
    return Status::kInvalidArg;
  }

  char chunk[128];
  uint64_t consumed = 0;
  while (consumed < buffer.size()) {
    const uint64_t remaining = buffer.size() - consumed;
    const uint64_t take = remaining > (sizeof(chunk) - 1U) ? (sizeof(chunk) - 1U) : remaining;
    mem::Copy(chunk, buffer.data() + consumed, take);
    chunk[take] = '\0';
    const Status status = io::ConsoleWriteTextActive(chunk);
    if (IsErr(status)) {
      return status;
    }
    consumed += take;
  }

  out_written = buffer.size();
  return Status::kOk;
}

Status NullRead(Node* /*node*/, uint64_t /*offset*/, std::span<uint8_t> /*buffer*/,
                uint64_t& out_read) noexcept {
  out_read = 0;
  return Status::kOk;
}

Status NullWrite(Node* /*node*/, uint64_t /*offset*/, std::span<const uint8_t> buffer,
                 uint64_t& out_written) noexcept {
  out_written = buffer.size();
  return Status::kOk;
}

Status ZeroRead(Node* /*node*/, uint64_t /*offset*/, std::span<uint8_t> buffer,
                uint64_t& out_read) noexcept {
  mem::Zero(buffer.data(), buffer.size());
  out_read = buffer.size();
  return Status::kOk;
}

Status RootLookup(Node* /*directory*/, const char* name, Node** out) noexcept {
  if (out == nullptr) {
    return Status::kInvalidArg;
  }
  *out = nullptr;
  if (name == nullptr) {
    return Status::kInvalidArg;
  }
  for (uint32_t index = 0; index < kDeviceCount; ++index) {
    if (mem::Same(name, g_devices[index]->name)) {
      *out = g_devices[index];
      return Status::kOk;
    }
  }
  return Status::kNotFound;
}

Status RootIterate(Node* /*directory*/, uint32_t index, DirEntry& entry, bool& found) noexcept {
  found = false;
  if (index >= kDeviceCount) {
    return Status::kOk;
  }
  const Node* node = g_devices[index];
  mem::Zero(&entry, sizeof(entry));
  const std::size_t length = mem::Length(node->name);
  if (length >= sizeof(entry.name)) {
    return Status::kInvalidArg;
  }
  mem::Copy(entry.name, node->name, length + 1U);
  entry.type = node->type;
  entry.size = node->size;
  found = true;
  return Status::kOk;
}

const NodeOps kRootOps{&RootLookup, nullptr, nullptr, &RootIterate};
const NodeOps kConsoleOps{nullptr, nullptr, &ConsoleWrite, nullptr};
const NodeOps kNullOps{nullptr, &NullRead, &NullWrite, nullptr};
const NodeOps kZeroOps{nullptr, &ZeroRead, &NullWrite, nullptr};

}  // namespace

Result<Node*> Create() noexcept {
  if (g_created) {
    return Result<Node*>::Ok(&g_root);
  }

  g_root = Node{"/", NodeType::kDirectory, 0, &kRootOps, nullptr, nullptr,
                kModeDirectory, 0, 0, 0};
  g_console = Node{"console", NodeType::kDevice, 0, &kConsoleOps, nullptr,
                   nullptr, kModeDevice, 0, 0, 0};
  g_null = Node{"null", NodeType::kDevice, 0, &kNullOps, nullptr, nullptr,
                kModeDevice, 0, 0, 0};
  g_zero = Node{"zero", NodeType::kDevice, 0, &kZeroOps, nullptr, nullptr,
                kModeDevice, 0, 0, 0};
  g_created = true;
  return Result<Node*>::Ok(&g_root);
}

}  // namespace og::fs::devfs
