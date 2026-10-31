#include <cstddef>
#include <cstdint>

extern "C" {

void* memset(void* d, int c, std::size_t n) {
  if (d == nullptr) {
    return nullptr;
  }
  auto* p = static_cast<unsigned char*>(d);
  for (std::size_t i = 0; i < n; ++i) {
    p[i] = static_cast<unsigned char>(c);
  }
  return d;
}

void* memcpy(void* d, const void* s, std::size_t n) {
  if (d == nullptr) {
    return nullptr;
  }
  if (s == nullptr) {
    return d;
  }
  auto* pd = static_cast<unsigned char*>(d);
  const auto* ps = static_cast<const unsigned char*>(s);
  for (std::size_t i = 0; i < n; ++i) {
    pd[i] = ps[i];
  }
  return d;
}

void* memmove(void* d, const void* s, std::size_t n) {
  if (d == nullptr) {
    return nullptr;
  }
  if (s == nullptr) {
    return d;
  }
  auto* pd = static_cast<unsigned char*>(d);
  const auto* ps = static_cast<const unsigned char*>(s);
  if (pd < ps) {
    for (std::size_t i = 0; i < n; ++i) {
      pd[i] = ps[i];
    }
  } else if (pd > ps) {
    for (std::size_t i = n; i > 0; --i) {
      pd[i - 1] = ps[i - 1];
    }
  }
  return d;
}

int memcmp(const void* a, const void* b, std::size_t n) {
  if (a == nullptr || b == nullptr) {
    return 0;
  }
  const auto* pa = static_cast<const unsigned char*>(a);
  const auto* pb = static_cast<const unsigned char*>(b);
  for (std::size_t i = 0; i < n; ++i) {
    if (pa[i] != pb[i]) {
      return pa[i] < pb[i] ? -1 : 1;
    }
  }
  return 0;
}

}  // extern C
