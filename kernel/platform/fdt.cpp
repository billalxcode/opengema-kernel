#include "og/platform/fdt.h"

#include "memory.h"

namespace og::platform::fdt {

namespace {

constexpr uint32_t kMagic = 0xD00DFEEDU;
constexpr uint32_t kTokenBeginNode = 1;
constexpr uint32_t kTokenEndNode = 2;
constexpr uint32_t kTokenProperty = 3;
constexpr uint32_t kTokenNop = 4;
constexpr uint32_t kTokenEnd = 9;
constexpr uint32_t kMaxPath = 128;

uint64_t g_base = 0;
uint32_t g_total = 0;
uint32_t g_struct = 0;
uint32_t g_struct_end = 0;
uint32_t g_strings = 0;
uint32_t g_strings_size = 0;
bool g_ready = false;

[[nodiscard]] uint32_t ReadU32(uint32_t offset) noexcept {
  const auto* data = reinterpret_cast<const uint8_t*>(g_base + offset);
  return (static_cast<uint32_t>(data[0]) << 24U) | (static_cast<uint32_t>(data[1]) << 16U) |
         (static_cast<uint32_t>(data[2]) << 8U) | static_cast<uint32_t>(data[3]);
}

[[nodiscard]] uint32_t Align4(uint32_t value) noexcept {
  return (value + 3U) & ~3U;
}

[[nodiscard]] bool Valid(uint32_t offset, uint32_t length) noexcept {
  if (offset > g_total) {
    return false;
  }
  return length <= (g_total - offset);
}

[[nodiscard]] const char* StructString(uint32_t offset) noexcept {
  if (offset >= g_struct_end) {
    return nullptr;
  }
  const auto* text = reinterpret_cast<const char*>(g_base + offset);
  for (uint32_t index = 0; (offset + index) < g_struct_end; ++index) {
    if (text[index] == '\0') {
      return text;
    }
  }
  return nullptr;
}

[[nodiscard]] const char* StringsEntry(uint32_t name_offset) noexcept {
  if (name_offset >= g_strings_size) {
    return nullptr;
  }
  const auto* text = reinterpret_cast<const char*>(g_base + g_strings + name_offset);
  for (uint32_t index = name_offset; index < g_strings_size; ++index) {
    if (text[index - name_offset] == '\0') {
      return text;
    }
  }
  return nullptr;
}

// Skips a node body starting right after its BEGIN_NODE token and name.
// Returns the offset after the matching END_NODE token.
[[nodiscard]] Result<uint32_t> SkipNode(uint32_t offset) noexcept {
  uint32_t depth = 1;
  while (offset + 4U <= g_struct_end) {
    const uint32_t token = ReadU32(offset);
    offset += 4U;
    switch (token) {
      case kTokenBeginNode: {
        const char* name = StructString(offset);
        if (name == nullptr) {
          return Result<uint32_t>::Err(Status::kInvalidArg);
        }
        offset = Align4(offset + static_cast<uint32_t>(mem::Length(name)) + 1U);
        ++depth;
        break;
      }
      case kTokenEndNode:
        --depth;
        if (depth == 0) {
          return Result<uint32_t>::Ok(offset);
        }
        break;
      case kTokenProperty: {
        if (!Valid(offset, 8)) {
          return Result<uint32_t>::Err(Status::kInvalidArg);
        }
        const uint32_t length = ReadU32(offset);
        offset = Align4(offset + 8U + length);
        break;
      }
      case kTokenNop:
        break;
      case kTokenEnd:
        return Result<uint32_t>::Err(Status::kNotFound);
      default:
        return Result<uint32_t>::Err(Status::kInvalidArg);
    }
  }
  return Result<uint32_t>::Err(Status::kInvalidArg);
}

void TruncatePath(char* path, std::size_t& length) noexcept {
  if (length <= 1U) {
    return;
  }
  std::size_t index = length;
  while (index > 0U) {
    --index;
    if (path[index] == '/') {
      length = index == 0U ? 1U : index;
      path[length] = '\0';
      return;
    }
  }
}

}  // namespace

Status Init(uint64_t address, uint64_t size) noexcept {
  if (address == 0) {
    return Status::kInvalidArg;
  }
  if (size < 40U) {
    return Status::kInvalidArg;
  }

  g_base = address;
  if (ReadU32(0) != kMagic) {
    return Status::kNoDevice;
  }
  const uint32_t total = ReadU32(4);
  if (total > size || total < 40U) {
    return Status::kInvalidArg;
  }
  g_total = total;

  const uint32_t off_struct = ReadU32(8);
  const uint32_t off_strings = ReadU32(12);
  const uint32_t size_strings = ReadU32(32);
  const uint32_t size_struct = ReadU32(36);
  if (!Valid(off_struct, size_struct)) {
    return Status::kInvalidArg;
  }
  if (!Valid(off_strings, size_strings)) {
    return Status::kInvalidArg;
  }

  g_struct = off_struct;
  g_struct_end = off_struct + size_struct;
  g_strings = off_strings;
  g_strings_size = size_strings;
  g_ready = true;
  return Status::kOk;
}

bool Ready() noexcept {
  return g_ready;
}

Result<uint32_t> FindNode(const char* path) noexcept {
  if (!g_ready) {
    return Result<uint32_t>::Err(Status::kNotInit);
  }
  if (path == nullptr) {
    return Result<uint32_t>::Err(Status::kInvalidArg);
  }

  char current[kMaxPath];
  std::size_t length = 0;
  current[0] = '\0';
  uint32_t depth = 0;
  uint32_t offset = g_struct;

  while (offset + 4U <= g_struct_end) {
    const uint32_t token_offset = offset;
    const uint32_t token = ReadU32(offset);
    offset += 4U;

    if (token == kTokenBeginNode) {
      const char* name = StructString(offset);
      if (name == nullptr) {
        return Result<uint32_t>::Err(Status::kInvalidArg);
      }
      const std::size_t name_length = mem::Length(name);
      if (depth == 0U) {
        current[0] = '/';
        current[1] = '\0';
        length = 1;
      } else if (name_length > 0U) {
        if (length + name_length + 2U >= kMaxPath) {
          return Result<uint32_t>::Err(Status::kInvalidArg);
        }
        if (length > 1U) {
          current[length] = '/';
          ++length;
        }
        mem::Copy(current + length, name, name_length);
        length += name_length;
        current[length] = '\0';
      }
      if (mem::Same(current, path)) {
        return Result<uint32_t>::Ok(token_offset);
      }
      offset = Align4(offset + static_cast<uint32_t>(name_length) + 1U);
      ++depth;
      continue;
    }

    if (token == kTokenEndNode) {
      if (depth > 0U) {
        --depth;
      }
      TruncatePath(current, length);
      continue;
    }

    if (token == kTokenProperty) {
      if (!Valid(offset, 8)) {
        return Result<uint32_t>::Err(Status::kInvalidArg);
      }
      const uint32_t property_length = ReadU32(offset);
      offset = Align4(offset + 8U + property_length);
      continue;
    }

    if (token == kTokenNop) {
      continue;
    }

    if (token == kTokenEnd) {
      break;
    }

    return Result<uint32_t>::Err(Status::kInvalidArg);
  }

  return Result<uint32_t>::Err(Status::kNotFound);
}

Result<std::span<const uint8_t>> FindProperty(uint32_t node, const char* name) noexcept {
  if (!g_ready) {
    return Result<std::span<const uint8_t>>::Err(Status::kNotInit);
  }
  if (name == nullptr) {
    return Result<std::span<const uint8_t>>::Err(Status::kInvalidArg);
  }
  if (ReadU32(node) != kTokenBeginNode) {
    return Result<std::span<const uint8_t>>::Err(Status::kInvalidArg);
  }

  uint32_t offset = node + 4U;
  const char* node_name = StructString(offset);
  if (node_name == nullptr) {
    return Result<std::span<const uint8_t>>::Err(Status::kInvalidArg);
  }
  offset = Align4(offset + static_cast<uint32_t>(mem::Length(node_name)) + 1U);

  while (offset + 4U <= g_struct_end) {
    const uint32_t token = ReadU32(offset);
    offset += 4U;
    if (token == kTokenProperty) {
      if (!Valid(offset, 8)) {
        return Result<std::span<const uint8_t>>::Err(Status::kInvalidArg);
      }
      const uint32_t length = ReadU32(offset);
      const uint32_t name_offset = ReadU32(offset + 4U);
      const uint32_t data_offset = offset + 8U;
      if (!Valid(data_offset, length)) {
        return Result<std::span<const uint8_t>>::Err(Status::kInvalidArg);
      }
      const char* property_name = StringsEntry(name_offset);
      if (property_name == nullptr) {
        return Result<std::span<const uint8_t>>::Err(Status::kInvalidArg);
      }
      if (mem::Same(property_name, name)) {
        return Result<std::span<const uint8_t>>::Ok(
          std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(g_base + data_offset),
                                   length));
      }
      offset = Align4(data_offset + length);
      continue;
    }
    break;
  }

  return Result<std::span<const uint8_t>>::Err(Status::kNotFound);
}

uint32_t ChildCount(uint32_t node) noexcept {
  if (!g_ready || ReadU32(node) != kTokenBeginNode) {
    return 0;
  }

  uint32_t offset = node + 4U;
  const char* name = StructString(offset);
  if (name == nullptr) {
    return 0;
  }
  offset = Align4(offset + static_cast<uint32_t>(mem::Length(name)) + 1U);

  uint32_t count = 0;
  while (offset + 4U <= g_struct_end) {
    const uint32_t token = ReadU32(offset);
    offset += 4U;
    if (token == kTokenBeginNode) {
      ++count;
      const char* child_name = StructString(offset);
      if (child_name == nullptr) {
        return count;
      }
      const Result<uint32_t> next =
        SkipNode(Align4(offset + static_cast<uint32_t>(mem::Length(child_name)) + 1U));
      if (next.is_err()) {
        return count;
      }
      offset = next.value();
      continue;
    }
    if (token == kTokenProperty) {
      if (!Valid(offset, 8)) {
        return count;
      }
      offset = Align4(offset + 8U + ReadU32(offset));
      continue;
    }
    if (token == kTokenEndNode || token == kTokenEnd) {
      break;
    }
    if (token != kTokenNop) {
      break;
    }
  }
  return count;
}

Result<uint32_t> Child(uint32_t node, uint32_t index) noexcept {
  if (!g_ready) {
    return Result<uint32_t>::Err(Status::kNotInit);
  }
  if (ReadU32(node) != kTokenBeginNode) {
    return Result<uint32_t>::Err(Status::kInvalidArg);
  }

  uint32_t offset = node + 4U;
  const char* name = StructString(offset);
  if (name == nullptr) {
    return Result<uint32_t>::Err(Status::kInvalidArg);
  }
  offset = Align4(offset + static_cast<uint32_t>(mem::Length(name)) + 1U);

  uint32_t seen = 0;
  while (offset + 4U <= g_struct_end) {
    const uint32_t token_offset = offset;
    const uint32_t token = ReadU32(offset);
    offset += 4U;
    if (token == kTokenBeginNode) {
      if (seen == index) {
        return Result<uint32_t>::Ok(token_offset);
      }
      ++seen;
      const char* child_name = StructString(offset);
      if (child_name == nullptr) {
        return Result<uint32_t>::Err(Status::kInvalidArg);
      }
      const Result<uint32_t> next =
        SkipNode(Align4(offset + static_cast<uint32_t>(mem::Length(child_name)) + 1U));
      if (next.is_err()) {
        return next;
      }
      offset = next.value();
      continue;
    }
    if (token == kTokenProperty) {
      if (!Valid(offset, 8)) {
        return Result<uint32_t>::Err(Status::kInvalidArg);
      }
      offset = Align4(offset + 8U + ReadU32(offset));
      continue;
    }
    if (token == kTokenEndNode || token == kTokenEnd) {
      break;
    }
    if (token != kTokenNop) {
      return Result<uint32_t>::Err(Status::kInvalidArg);
    }
  }

  return Result<uint32_t>::Err(Status::kNotFound);
}

Result<const char*> NodeName(uint32_t node) noexcept {
  if (!g_ready) {
    return Result<const char*>::Err(Status::kNotInit);
  }
  if ((node + 4U) > g_struct_end) {
    return Result<const char*>::Err(Status::kInvalidArg);
  }
  if (ReadU32(node) != kTokenBeginNode) {
    return Result<const char*>::Err(Status::kInvalidArg);
  }
  const char* name = StructString(node + 4U);
  if (name == nullptr) {
    return Result<const char*>::Err(Status::kInvalidArg);
  }
  return Result<const char*>::Ok(name);
}

}  // namespace og::platform::fdt
