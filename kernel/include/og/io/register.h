#pragma once

// Canonical owner of register field encode and decode.
// Built on libs/core/bit.h so the shift math has one implementation.

#include <cstdint>

#include "bit.h"

namespace og::io {

template <typename T, unsigned kPos, unsigned kWidth>
class Field {
 public:
  static_assert(kWidth > 0U, "field width must be positive");
  static_assert(kPos + kWidth <= sizeof(T) * 8U, "field exceeds type width");

  [[nodiscard]] static constexpr T Decode(T reg) noexcept {
    return og::bits::Extract<kPos, kWidth>(reg);
  }

  [[nodiscard]] static constexpr T Encode(T value) noexcept {
    return static_cast<T>(value & og::bits::Mask<T>(kWidth));
  }

  [[nodiscard]] static constexpr T Insert(T reg, T value) noexcept {
    return og::bits::Insert<kPos, kWidth>(reg, value);
  }

  [[nodiscard]] static constexpr T Mask() noexcept {
    return static_cast<T>(og::bits::Mask<T>(kWidth) << kPos);
  }
};

}  // namespace og::io
