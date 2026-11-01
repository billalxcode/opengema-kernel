#pragma once

// 32 bit framebuffer console. The driver that owns the pixels fills the
// backing memory, then hands it to this class and gets an io::Console back.
// Pixel format is B8G8R8A8 little endian, matching virtio-gpu.

#include <cstdint>
#include <span>

#include "og/core/status.h"
#include "og/io/console.h"

namespace og::io {

constexpr uint32_t kGlyphWidth = 8;
constexpr uint32_t kGlyphHeight = 8;

// AARRGGBB in a 32 bit word, BGR in memory. Alpha stays opaque.
[[nodiscard]] constexpr uint32_t Rgb(uint8_t red, uint8_t green, uint8_t blue) noexcept {
  return 0xFF000000U | (static_cast<uint32_t>(red) << 16U) |
         (static_cast<uint32_t>(green) << 8U) | static_cast<uint32_t>(blue);
}

class FramebufferConsole {
 public:
  // Called after the console changed pixels, with the damaged rectangle.
  using PresentFn = Status (*)(void* context, uint32_t x, uint32_t y,
                               uint32_t width, uint32_t height);

  constexpr FramebufferConsole() noexcept = default;
  ~FramebufferConsole() = default;

  FramebufferConsole(const FramebufferConsole&) = delete;
  FramebufferConsole& operator=(const FramebufferConsole&) = delete;

  [[nodiscard]] Status init(void* pixels, uint32_t width, uint32_t height,
                            uint32_t stride, uint32_t foreground,
                            uint32_t background, PresentFn present,
                            void* context) noexcept;

  [[nodiscard]] Status clear(uint32_t color) noexcept;
  [[nodiscard]] Status write(std::span<const uint8_t> bytes) noexcept;
  [[nodiscard]] Status present() noexcept;

  [[nodiscard]] Console console() noexcept { return Console(&WriteTrampoline, this); }
  [[nodiscard]] bool is_ready() const noexcept { return ready_; }
  [[nodiscard]] uint32_t width() const noexcept { return width_; }
  [[nodiscard]] uint32_t height() const noexcept { return height_; }
  [[nodiscard]] uint32_t stride() const noexcept { return stride_; }

 private:
  static Status WriteTrampoline(void* context, std::span<const uint8_t> bytes) noexcept;
  void draw_glyph(char value) noexcept;
  void newline() noexcept;
  void scroll() noexcept;
  void fill_row(uint32_t row, uint32_t color) noexcept;
  void mark_dirty(uint32_t x, uint32_t y, uint32_t width, uint32_t height) noexcept;
  void mark_all_dirty() noexcept;

  uint8_t* pixels_ = nullptr;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  uint32_t stride_ = 0;
  uint32_t cursor_x_ = 0;
  uint32_t cursor_y_ = 0;
  uint32_t foreground_ = 0;
  uint32_t background_ = 0;
  uint32_t dirty_min_x_ = 0;
  uint32_t dirty_min_y_ = 0;
  uint32_t dirty_max_x_ = 0;
  uint32_t dirty_max_y_ = 0;
  PresentFn present_ = nullptr;
  void* present_context_ = nullptr;
  bool ready_ = false;
};

}  // namespace og::io
