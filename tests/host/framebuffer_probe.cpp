#include <cstdint>
#include <cstdio>
#include <span>

#include "og/io/framebuffer.h"

namespace {

constexpr uint32_t kWidth = 64;
constexpr uint32_t kHeight = 32;
constexpr uint32_t kStride = kWidth * og::io::kBytesPerPixel;
constexpr uint32_t kForeground = og::io::Rgb(255, 255, 255);
constexpr uint32_t kBackground = og::io::Rgb(0, 0, 0);

uint32_t g_pixels[kWidth * kHeight];

uint32_t PixelAt(uint32_t x, uint32_t y) {
  return g_pixels[(static_cast<std::size_t>(y) * kWidth) + x];
}

bool ScreenIsBackground() {
  for (uint32_t index = 0; index < kWidth * kHeight; ++index) {
    if (g_pixels[index] != kBackground) {
      return false;
    }
  }
  return true;
}

bool TopLeftHasInk() {
  for (uint32_t y = 0; y < og::io::kGlyphHeight; ++y) {
    for (uint32_t x = 0; x < og::io::kGlyphWidth; ++x) {
      if (PixelAt(x, y) == kForeground) {
        return true;
      }
    }
  }
  return false;
}

int Fail(const char* message) {
  std::printf("framebuffer probe: %s\n", message);
  return 1;
}

}  // namespace

int main() {
  og::io::FramebufferConsole console;
  if (!og::IsOk(console.init(g_pixels, kWidth, kHeight, kStride, kForeground,
                             kBackground, nullptr, nullptr))) {
    return Fail("init");
  }

  const char glyph[] = "A";
  if (!og::IsOk(console.write(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(glyph), 1)))) {
    return Fail("write glyph");
  }
  if (!TopLeftHasInk()) {
    return Fail("glyph did not draw");
  }

  const char clear[] = "\x1b[2J\x1b[H";
  if (!og::IsOk(console.write(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(clear), sizeof(clear) - 1U)))) {
    return Fail("write clear");
  }
  if (!ScreenIsBackground()) {
    return Fail("clear left pixels behind");
  }

  const char after[] = "B";
  if (!og::IsOk(console.write(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(after), 1)))) {
    return Fail("write after clear");
  }
  if (!TopLeftHasInk()) {
    return Fail("cursor did not return home");
  }

  if (!og::IsOk(console.write(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(clear), sizeof(clear) - 1U)))) {
    return Fail("write clear twice");
  }
  if (!ScreenIsBackground()) {
    return Fail("second clear left pixels behind");
  }

  std::printf("framebuffer probe: OK\n");
  return 0;
}
