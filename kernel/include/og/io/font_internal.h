#pragma once

// Private framebuffer font access. Not part of the public console API.

#include <cstdint>

namespace og::io::detail {

[[nodiscard]] const uint8_t* GlyphFor(char value) noexcept;

}  // namespace og::io::detail
