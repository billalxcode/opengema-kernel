#pragma once

// Canonical owner of memory and string primitives for the kernel.
// No other module may define these symbols.
//
// The extern "C" symbols exist because the compiler emits calls to them
// for struct copies and array initialization. The og::mem wrappers are
// the names kernel code should use.

#include <cstddef>

extern "C" {

void* memcpy(void* dst, const void* src, std::size_t size);
void* memmove(void* dst, const void* src, std::size_t size);
void* memset(void* dst, int value, std::size_t size);
int memcmp(const void* lhs, const void* rhs, std::size_t size);

std::size_t strlen(const char* text);
int strcmp(const char* lhs, const char* rhs);
int strncmp(const char* lhs, const char* rhs, std::size_t size);

}  // extern "C"

namespace og::mem {

inline void Copy(void* dst, const void* src, std::size_t size) {
  (void)memcpy(dst, src, size);
}

inline void Move(void* dst, const void* src, std::size_t size) {
  (void)memmove(dst, src, size);
}

inline void Fill(void* dst, int value, std::size_t size) {
  (void)memset(dst, value, size);
}

inline void Zero(void* dst, std::size_t size) {
  (void)memset(dst, 0, size);
}

[[nodiscard]] inline bool Equal(const void* lhs, const void* rhs, std::size_t size) {
  return memcmp(lhs, rhs, size) == 0;
}

[[nodiscard]] inline std::size_t Length(const char* text) {
  return strlen(text);
}

[[nodiscard]] inline bool Same(const char* lhs, const char* rhs) {
  return strcmp(lhs, rhs) == 0;
}

}  // namespace og::mem
