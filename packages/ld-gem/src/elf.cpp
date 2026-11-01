#include "ldgem/elf.h"

namespace ldgem::elf {

u16 Read16(const u8* data, u64 offset) noexcept {
  return static_cast<u16>(static_cast<u16>(data[offset]) |
                          static_cast<u16>(static_cast<u16>(data[offset + 1U]) << 8U));
}

u32 Read32(const u8* data, u64 offset) noexcept {
  u32 value = 0;
  for (u32 index = 0; index < 4U; ++index) {
    value |= static_cast<u32>(data[offset + index]) << (index * 8U);
  }
  return value;
}

u64 Read64(const u8* data, u64 offset) noexcept {
  u64 value = 0;
  for (u32 index = 0; index < 8U; ++index) {
    value |= static_cast<u64>(data[offset + index]) << (index * 8U);
  }
  return value;
}

bool Same(const char* lhs, const char* rhs) noexcept {
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }
  while (*lhs != '\0' && *lhs == *rhs) {
    ++lhs;
    ++rhs;
  }
  return *lhs == *rhs;
}

bool StartsWith(const char* text, const char* prefix) noexcept {
  if (text == nullptr || prefix == nullptr) {
    return false;
  }
  while (*prefix != '\0') {
    if (*text != *prefix) {
      return false;
    }
    ++text;
    ++prefix;
  }
  return true;
}

u32 HashSymbolName(const char* name) noexcept {
  u32 hash = 0;
  if (name == nullptr) {
    return 0;
  }
  while (*name != '\0') {
    hash = (hash << 4U) + static_cast<u32>(static_cast<u8>(*name));
    const u32 high = hash & 0xF0000000U;
    if (high != 0U) {
      hash ^= high >> 24U;
    }
    hash &= ~high;
    ++name;
  }
  return hash;
}

u64 AlignUp(u64 value, u64 align) noexcept {
  if (align == 0U) {
    return value;
  }
  return (value + align - 1U) & ~(align - 1U);
}

}  // namespace ldgem::elf
