#include "og/platform/fdt.h"

#include "memory.h"
#include "og/platform/fdt_internal.h"

namespace og::platform::fdt {

namespace {

constexpr uint32_t kMagic = 0xD00DFEEDU;

uint64_t g_base = 0;
uint32_t g_total = 0;
uint32_t g_struct = 0;
uint32_t g_struct_end = 0;
uint32_t g_strings = 0;
uint32_t g_strings_size = 0;
bool g_ready = false;

}  // namespace

namespace detail {

uint32_t ReadU32(uint32_t offset) noexcept {
  const auto* data = reinterpret_cast<const uint8_t*>(g_base + offset);
  return (static_cast<uint32_t>(data[0]) << 24U) |
         (static_cast<uint32_t>(data[1]) << 16U) |
         (static_cast<uint32_t>(data[2]) << 8U) |
         static_cast<uint32_t>(data[3]);
}

uint32_t Align4(uint32_t value) noexcept {
  return (value + 3U) & ~3U;
}

bool Valid(uint32_t offset, uint32_t length) noexcept {
  if (offset > g_total) {
    return false;
  }
  return length <= (g_total - offset);
}

const char* StructString(uint32_t offset) noexcept {
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

const char* StringsEntry(uint32_t name_offset) noexcept {
  if (name_offset >= g_strings_size) {
    return nullptr;
  }
  const auto* text =
    reinterpret_cast<const char*>(g_base + g_strings + name_offset);
  for (uint32_t index = name_offset; index < g_strings_size; ++index) {
    if (text[index - name_offset] == '\0') {
      return text;
    }
  }
  return nullptr;
}

Result<uint32_t> NodeBodyOffset(uint32_t node) noexcept {
  if ((node + 4U) > g_struct_end || ReadU32(node) != kTokenBeginNode) {
    return Result<uint32_t>::Err(Status::kInvalidArg);
  }
  const char* name = StructString(node + 4U);
  if (name == nullptr) {
    return Result<uint32_t>::Err(Status::kInvalidArg);
  }
  return Result<uint32_t>::Ok(
    Align4(node + 4U + static_cast<uint32_t>(mem::Length(name)) + 1U));
}

Result<uint32_t> SkipNode(uint32_t offset) noexcept {
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
        offset =
          Align4(offset + static_cast<uint32_t>(mem::Length(name)) + 1U);
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

uint32_t StructStart() noexcept {
  return g_struct;
}

uint32_t StructEnd() noexcept {
  return g_struct_end;
}

const uint8_t* Base() noexcept {
  return reinterpret_cast<const uint8_t*>(g_base);
}

}  // namespace detail

Status Init(uint64_t address, uint64_t size) noexcept {
  if (address == 0) {
    return Status::kInvalidArg;
  }
  if (size < 40U) {
    return Status::kInvalidArg;
  }

  g_base = address;
  if (detail::ReadU32(0) != kMagic) {
    return Status::kNoDevice;
  }
  const uint32_t total = detail::ReadU32(4);
  if (total > size || total < 40U) {
    return Status::kInvalidArg;
  }
  g_total = total;

  const uint32_t off_struct = detail::ReadU32(8);
  const uint32_t off_strings = detail::ReadU32(12);
  const uint32_t size_strings = detail::ReadU32(32);
  const uint32_t size_struct = detail::ReadU32(36);
  if (!detail::Valid(off_struct, size_struct)) {
    return Status::kInvalidArg;
  }
  if (!detail::Valid(off_strings, size_strings)) {
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

}  // namespace og::platform::fdt
