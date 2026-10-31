#include "memory.h"

namespace {

// Byte loops keep this correct for every alignment. Optimize later only
// with a benchmark, not by guess. Guard clauses handle null first.

}  // namespace

extern "C" {

void* memcpy(void* dst, const void* src, std::size_t size) {
  if (dst == nullptr) {
    return nullptr;
  }
  if (src == nullptr) {
    return dst;
  }
  auto* out = static_cast<unsigned char*>(dst);
  const auto* in = static_cast<const unsigned char*>(src);
  for (std::size_t i = 0; i < size; ++i) {
    out[i] = in[i];
  }
  return dst;
}

void* memmove(void* dst, const void* src, std::size_t size) {
  if (dst == nullptr) {
    return nullptr;
  }
  if (src == nullptr) {
    return dst;
  }
  auto* out = static_cast<unsigned char*>(dst);
  const auto* in = static_cast<const unsigned char*>(src);
  if (out == in) {
    return dst;
  }
  if (out < in) {
    for (std::size_t i = 0; i < size; ++i) {
      out[i] = in[i];
    }
    return dst;
  }
  for (std::size_t i = size; i > 0; --i) {
    out[i - 1] = in[i - 1];
  }
  return dst;
}

void* memset(void* dst, int value, std::size_t size) {
  if (dst == nullptr) {
    return nullptr;
  }
  auto* out = static_cast<unsigned char*>(dst);
  const unsigned char byte = static_cast<unsigned char>(value);
  for (std::size_t i = 0; i < size; ++i) {
    out[i] = byte;
  }
  return dst;
}

int memcmp(const void* lhs, const void* rhs, std::size_t size) {
  if (lhs == nullptr || rhs == nullptr) {
    return 0;
  }
  const auto* a = static_cast<const unsigned char*>(lhs);
  const auto* b = static_cast<const unsigned char*>(rhs);
  for (std::size_t i = 0; i < size; ++i) {
    if (a[i] == b[i]) {
      continue;
    }
    return a[i] < b[i] ? -1 : 1;
  }
  return 0;
}

std::size_t strlen(const char* text) {
  if (text == nullptr) {
    return 0;
  }
  std::size_t length = 0;
  while (text[length] != '\0') {
    ++length;
  }
  return length;
}

int strcmp(const char* lhs, const char* rhs) {
  if (lhs == nullptr || rhs == nullptr) {
    return 0;
  }
  std::size_t i = 0;
  while (lhs[i] != '\0' && rhs[i] != '\0') {
    if (lhs[i] != rhs[i]) {
      return static_cast<unsigned char>(lhs[i]) < static_cast<unsigned char>(rhs[i]) ? -1 : 1;
    }
    ++i;
  }
  if (lhs[i] == rhs[i]) {
    return 0;
  }
  return lhs[i] == '\0' ? -1 : 1;
}

int strncmp(const char* lhs, const char* rhs, std::size_t size) {
  if (lhs == nullptr || rhs == nullptr) {
    return 0;
  }
  for (std::size_t i = 0; i < size; ++i) {
    if (lhs[i] != rhs[i]) {
      return static_cast<unsigned char>(lhs[i]) < static_cast<unsigned char>(rhs[i]) ? -1 : 1;
    }
    if (lhs[i] == '\0') {
      return 0;
    }
  }
  return 0;
}

}  // extern "C"
