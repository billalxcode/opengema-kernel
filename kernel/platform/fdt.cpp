#include "og/platform/fdt.h"

#include "memory.h"
#include "og/platform/fdt_internal.h"

namespace og::platform::fdt {

using namespace detail;

namespace {

constexpr uint32_t kMaxPath = 128;

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

Result<uint32_t> FindNode(const char* path) noexcept {
  if (!Ready()) {
    return Result<uint32_t>::Err(Status::kNotInit);
  }
  if (path == nullptr) {
    return Result<uint32_t>::Err(Status::kInvalidArg);
  }

  char current[kMaxPath];
  std::size_t length = 0;
  current[0] = '\0';
  uint32_t depth = 0;
  uint32_t offset = StructStart();

  while (offset + 4U <= StructEnd()) {
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

uint32_t ChildCount(uint32_t node) noexcept {
  if (!Ready()) {
    return 0;
  }
  const Result<uint32_t> body = NodeBodyOffset(node);
  if (body.is_err()) {
    return 0;
  }
  uint32_t offset = body.value();

  uint32_t count = 0;
  while (offset + 4U <= StructEnd()) {
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
  if (!Ready()) {
    return Result<uint32_t>::Err(Status::kNotInit);
  }
  const Result<uint32_t> body = NodeBodyOffset(node);
  if (body.is_err()) {
    return Result<uint32_t>::Err(body.status());
  }
  uint32_t offset = body.value();

  uint32_t seen = 0;
  while (offset + 4U <= StructEnd()) {
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
  if (!Ready()) {
    return Result<const char*>::Err(Status::kNotInit);
  }
  if ((node + 4U) > StructEnd()) {
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
