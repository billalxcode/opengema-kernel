// Compiler runtime helpers for freestanding user space builds. The compiler
// may emit calls to these for aggregate initialization and struct copies, so
// every user program needs them. They are plain functions, not syscalls.

#include "kgema/types.h"

extern "C" {

void* memset(void* destination, int value, kgema::usize size) {
  auto* bytes = static_cast<unsigned char*>(destination);
  for (kgema::usize index = 0; index < size; ++index) {
    bytes[index] = static_cast<unsigned char>(value);
  }
  return destination;
}

void* memcpy(void* destination, const void* source, kgema::usize size) {
  auto* out = static_cast<unsigned char*>(destination);
  const auto* in = static_cast<const unsigned char*>(source);
  for (kgema::usize index = 0; index < size; ++index) {
    out[index] = in[index];
  }
  return destination;
}

void* memmove(void* destination, const void* source, kgema::usize size) {
  auto* out = static_cast<unsigned char*>(destination);
  const auto* in = static_cast<const unsigned char*>(source);
  if (out < in) {
    for (kgema::usize index = 0; index < size; ++index) {
      out[index] = in[index];
    }
  } else {
    for (kgema::usize index = size; index > 0; --index) {
      out[index - 1U] = in[index - 1U];
    }
  }
  return destination;
}

}  // extern "C"
