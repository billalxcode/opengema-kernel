#include "og/io/framebuffer.h"

#include "memory.h"
#include "og/io/font_internal.h"

namespace og::io {

void FramebufferConsole::draw_glyph(char value) noexcept {
  if ((cursor_x_ + kGlyphWidth) > width_) {
    newline();
  }
  const uint8_t* glyph = detail::GlyphFor(value);
  for (uint32_t row = 0; row < kGlyphHeight; ++row) {
    auto* line = reinterpret_cast<uint32_t*>(
      pixels_ + (static_cast<std::size_t>(cursor_y_ + row) * stride_) +
      (static_cast<std::size_t>(cursor_x_) * kBytesPerPixel));
    const uint8_t bits = glyph[row];
    for (uint32_t column = 0; column < kGlyphWidth; ++column) {
      line[column] = (bits & (1U << column)) != 0U ? foreground_ : background_;
    }
  }
  mark_dirty(cursor_x_, cursor_y_, kGlyphWidth, kGlyphHeight);
  cursor_x_ += kGlyphWidth;
}

void FramebufferConsole::newline() noexcept {
  cursor_x_ = 0;
  cursor_y_ += kLineHeight;
  if ((cursor_y_ + kGlyphHeight) > height_) {
    scroll();
    cursor_y_ -= kLineHeight;
  }
}

void FramebufferConsole::scroll() noexcept {
  if (height_ <= kLineHeight) {
    for (uint32_t row = 0; row < height_; ++row) {
      fill_row(row, background_);
    }
    mark_all_dirty();
    return;
  }
  for (uint32_t row = kLineHeight; row < height_; ++row) {
    mem::Copy(pixels_ + (static_cast<std::size_t>(row - kLineHeight) * stride_),
              pixels_ + (static_cast<std::size_t>(row) * stride_), stride_);
  }
  for (uint32_t row = height_ - kLineHeight; row < height_; ++row) {
    fill_row(row, background_);
  }
  mark_all_dirty();
}

void FramebufferConsole::fill_row(uint32_t row, uint32_t color) noexcept {
  auto* line = reinterpret_cast<uint32_t*>(
    pixels_ + (static_cast<std::size_t>(row) * stride_));
  for (uint32_t column = 0; column < width_; ++column) {
    line[column] = color;
  }
}

void FramebufferConsole::mark_dirty(uint32_t x, uint32_t y, uint32_t width,
                                    uint32_t height) noexcept {
  if (width == 0 || height == 0) {
    return;
  }
  const uint32_t right = x + width;
  const uint32_t bottom = y + height;
  if (dirty_max_x_ <= dirty_min_x_ || dirty_max_y_ <= dirty_min_y_) {
    dirty_min_x_ = x;
    dirty_min_y_ = y;
    dirty_max_x_ = right;
    dirty_max_y_ = bottom;
    return;
  }
  if (x < dirty_min_x_) {
    dirty_min_x_ = x;
  }
  if (y < dirty_min_y_) {
    dirty_min_y_ = y;
  }
  if (right > dirty_max_x_) {
    dirty_max_x_ = right;
  }
  if (bottom > dirty_max_y_) {
    dirty_max_y_ = bottom;
  }
}

void FramebufferConsole::mark_all_dirty() noexcept {
  dirty_min_x_ = 0;
  dirty_min_y_ = 0;
  dirty_max_x_ = width_;
  dirty_max_y_ = height_;
}

}  // namespace og::io
