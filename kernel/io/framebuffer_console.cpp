#include "og/io/framebuffer.h"

namespace og::io {

Status FramebufferConsole::init(void* pixels, uint32_t width, uint32_t height,
                                uint32_t stride, uint32_t foreground,
                                uint32_t background, PresentFn present,
                                void* context) noexcept {
  if (pixels == nullptr) {
    return Status::kInvalidArg;
  }
  if (width < kGlyphWidth || height < kLineHeight) {
    return Status::kInvalidArg;
  }
  if (stride < (width * kBytesPerPixel)) {
    return Status::kInvalidArg;
  }

  pixels_ = static_cast<uint8_t*>(pixels);
  width_ = width;
  height_ = height;
  stride_ = stride;
  cursor_x_ = 0;
  cursor_y_ = 0;
  foreground_ = foreground;
  background_ = background;
  present_ = present;
  present_context_ = context;
  dirty_min_x_ = 0;
  dirty_min_y_ = 0;
  dirty_max_x_ = 0;
  dirty_max_y_ = 0;
  ready_ = true;
  return clear(background_);
}

Status FramebufferConsole::clear(uint32_t color) noexcept {
  if (!ready_) {
    return Status::kNotInit;
  }
  for (uint32_t row = 0; row < height_; ++row) {
    fill_row(row, color);
  }
  cursor_x_ = 0;
  cursor_y_ = 0;
  mark_all_dirty();
  return Status::kOk;
}

Status FramebufferConsole::write(std::span<const uint8_t> bytes) noexcept {
  if (!ready_) {
    return Status::kNotInit;
  }
  if (bytes.empty()) {
    return Status::kOk;
  }

  for (const uint8_t raw : bytes) {
    if (ansi_state_ != 0U) {
      if (ansi_state_ == 1U) {
        ansi_state_ = raw == '[' ? 2U : 0U;
        continue;
      }
      if (ansi_state_ == 2U) {
        if (raw >= '0' && raw <= '9') {
          ansi_param_ = static_cast<uint32_t>(raw - '0');
          ansi_state_ = 3U;
        } else if (raw == 'J') {
          (void)clear(background_);
          ansi_state_ = 0U;
        } else if (raw == 'H') {
          cursor_x_ = 0;
          cursor_y_ = 0;
          ansi_state_ = 0U;
        } else {
          ansi_state_ = 0U;
        }
        continue;
      }
      if (ansi_state_ == 3U) {
        if (raw >= '0' && raw <= '9') {
          ansi_param_ = (ansi_param_ * 10U) + static_cast<uint32_t>(raw - '0');
          continue;
        }
        if (raw == 'J') {
          if (ansi_param_ == 2U || ansi_param_ == 3U) {
            (void)clear(background_);
          }
        } else if (raw == 'H' && (ansi_param_ == 0U || ansi_param_ == 1U)) {
          cursor_x_ = 0;
          cursor_y_ = 0;
        }
        ansi_state_ = 0U;
        ansi_param_ = 0U;
        continue;
      }
    }
    if (raw == 0x1BU) {
      ansi_state_ = 1U;
      ansi_param_ = 0U;
      continue;
    }
    const char value = static_cast<char>(raw);
    if (value == '\r') {
      cursor_x_ = 0;
      continue;
    }
    if (value == '\n') {
      newline();
      continue;
    }
    if (value == '\t') {
      for (uint32_t index = 0; index < 4U; ++index) {
        draw_glyph(' ');
      }
      continue;
    }
    if (raw < kFirstGlyph || raw > kLastGlyph) {
      continue;
    }
    draw_glyph(value);
  }

  return present();
}

Status FramebufferConsole::present() noexcept {
  if (!ready_) {
    return Status::kNotInit;
  }
  if (dirty_max_x_ <= dirty_min_x_ || dirty_max_y_ <= dirty_min_y_) {
    return Status::kOk;
  }
  if (present_ == nullptr) {
    dirty_min_x_ = 0;
    dirty_min_y_ = 0;
    dirty_max_x_ = 0;
    dirty_max_y_ = 0;
    return Status::kOk;
  }
  const Status status =
    present_(present_context_, dirty_min_x_, dirty_min_y_,
             dirty_max_x_ - dirty_min_x_, dirty_max_y_ - dirty_min_y_);
  if (IsOk(status)) {
    dirty_min_x_ = 0;
    dirty_min_y_ = 0;
    dirty_max_x_ = 0;
    dirty_max_y_ = 0;
  }
  return status;
}

Status FramebufferConsole::WriteTrampoline(
  void* context, std::span<const uint8_t> bytes) noexcept {
  if (context == nullptr) {
    return Status::kInvalidArg;
  }
  return static_cast<FramebufferConsole*>(context)->write(bytes);
}

}  // namespace og::io
