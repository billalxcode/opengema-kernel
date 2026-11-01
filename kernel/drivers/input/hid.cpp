#include "og/drivers/hid.h"

namespace og::drivers::hid {

namespace {

bool ContainsKey(const uint8_t* keys, uint8_t value) noexcept {
  for (uint32_t index = 0; index < 6U; ++index) {
    if (keys[index] == value) {
      return true;
    }
  }
  return false;
}

}  // namespace

void Decoder::SetSink(EventFn sink, void* context) noexcept {
  sink_ = sink;
  context_ = context;
}

void Decoder::Emit(EventType type, uint16_t code, int32_t value) noexcept {
  if (sink_ == nullptr) {
    return;
  }
  sink_(context_, Event{type, code, value});
}

Status Decoder::KeyboardReport(std::span<const uint8_t> report) noexcept {
  if (report.size() < kKeyboardReportSize) {
    return Status::kInvalidArg;
  }

  const uint8_t modifiers = report[0];
  const uint8_t* keys = report.data() + 2U;

  for (uint32_t bit = 0; bit < 8U; ++bit) {
    const uint8_t mask = static_cast<uint8_t>(1U << bit);
    if ((modifiers ^ modifiers_) & mask) {
      const int32_t value = (modifiers & mask) != 0U ? 1 : 0;
      Emit(EventType::kKey, static_cast<uint16_t>(kModifierFirstUsage + bit),
           value);
    }
  }

  for (uint32_t index = 0; index < 6U; ++index) {
    const uint8_t key = keys[index];
    if (key != 0U && !ContainsKey(keys_, key)) {
      Emit(EventType::kKey, key, 1);
    }
  }
  for (uint32_t index = 0; index < 6U; ++index) {
    const uint8_t key = keys_[index];
    if (key != 0U && !ContainsKey(keys, key)) {
      Emit(EventType::kKey, key, 0);
    }
  }

  modifiers_ = modifiers;
  for (uint32_t index = 0; index < 6U; ++index) {
    keys_[index] = keys[index];
  }
  return Status::kOk;
}

Status Decoder::MouseReport(std::span<const uint8_t> report) noexcept {
  if (report.size() < kMouseReportSize) {
    return Status::kInvalidArg;
  }

  const uint8_t buttons = report[0];
  for (uint32_t bit = 0; bit < 8U; ++bit) {
    const uint8_t mask = static_cast<uint8_t>(1U << bit);
    if ((buttons ^ buttons_) & mask) {
      const int32_t value = (buttons & mask) != 0U ? 1 : 0;
      Emit(EventType::kButton, static_cast<uint16_t>(bit + 1U), value);
    }
  }
  buttons_ = buttons;

  const int32_t dx = static_cast<int8_t>(report[1]);
  const int32_t dy = static_cast<int8_t>(report[2]);
  if (dx != 0) {
    Emit(EventType::kAxis, kAxisX, dx);
  }
  if (dy != 0) {
    Emit(EventType::kAxis, kAxisY, dy);
  }
  if (report.size() > kMouseReportSize) {
    const int32_t wheel = static_cast<int8_t>(report[3]);
    if (wheel != 0) {
      Emit(EventType::kAxis, kAxisWheel, wheel);
    }
  }
  return Status::kOk;
}

char AsciiForKey(uint16_t usage, bool shift, bool caps) noexcept {
  if (usage >= 0x04U && usage <= 0x1DU) {
    const char base = static_cast<char>('a' + (usage - 0x04U));
    const bool upper = shift != caps;
    return upper ? static_cast<char>(base - 'a' + 'A') : base;
  }
  if (usage >= 0x1EU && usage <= 0x27U) {
    const uint32_t index = usage - 0x1EU;
    constexpr char kDigits[] = "1234567890";
    constexpr char kShifted[] = "!@#$%^&*()";
    return shift ? kShifted[index] : kDigits[index];
  }
  switch (usage) {
    case 0x28:
      return '\n';
    case 0x29:
      return 0x1B;
    case 0x2A:
      return '\b';
    case 0x2B:
      return '\t';
    case 0x2C:
      return ' ';
    case 0x2D:
      return shift ? '_' : '-';
    case 0x2E:
      return shift ? '+' : '=';
    case 0x2F:
      return shift ? '{' : '[';
    case 0x30:
      return shift ? '}' : ']';
    case 0x31:
      return shift ? '|' : '\\';
    case 0x33:
      return shift ? ':' : ';';
    case 0x34:
      return shift ? '"' : '\'';
    case 0x35:
      return shift ? '~' : '`';
    case 0x36:
      return shift ? '<' : ',';
    case 0x37:
      return shift ? '>' : '.';
    case 0x38:
      return shift ? '?' : '/';
    default:
      return 0;
  }
}

}  // namespace og::drivers::hid
