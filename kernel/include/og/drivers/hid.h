#pragma once

// Generic HID boot-protocol decoder for keyboards and mice. The transport
// (USB, virtio-input, or a board-specific port) only has to feed reports.

#include <cstdint>
#include <span>

#include "og/core/status.h"

namespace og::drivers::hid {

enum class EventType : uint8_t {
  kKey = 0,
  kButton = 1,
  kAxis = 2,
};

enum Axis : uint16_t {
  kAxisX = 0,
  kAxisY = 1,
  kAxisWheel = 2,
};

constexpr uint16_t kModifierFirstUsage = 0xE0;
constexpr uint32_t kKeyboardReportSize = 8;
constexpr uint32_t kMouseReportSize = 3;

struct Event {
  EventType type;
  uint16_t code;
  int32_t value;
};

// Maps a HID keyboard usage to ASCII. Returns 0 when the key has no text
// representation. Caps lock applies to letters only.
[[nodiscard]] char AsciiForKey(uint16_t usage, bool shift,
                               bool caps) noexcept;

using EventFn = void (*)(void* context, const Event& event) noexcept;

// Decodes boot keyboard and boot mouse reports. Key events carry HID usage
// IDs; button events use 1-based button numbers; axis events use Axis.
class Decoder {
 public:
  Decoder() noexcept = default;

  void SetSink(EventFn sink, void* context) noexcept;

  [[nodiscard]] Status KeyboardReport(std::span<const uint8_t> report) noexcept;
  [[nodiscard]] Status MouseReport(std::span<const uint8_t> report) noexcept;

 private:
  void Emit(EventType type, uint16_t code, int32_t value) noexcept;

  EventFn sink_ = nullptr;
  void* context_ = nullptr;
  uint8_t keys_[6]{};
  uint8_t modifiers_ = 0;
  uint8_t buttons_ = 0;
};

}  // namespace og::drivers::hid
