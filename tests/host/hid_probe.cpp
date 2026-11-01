#include <cstdint>
#include <cstdio>
#include <span>

#include "og/drivers/hid.h"

namespace {

struct Capture {
  og::drivers::hid::Event events[16];
  uint32_t count;
};

void Sink(void* context, const og::drivers::hid::Event& event) noexcept {
  auto* capture = static_cast<Capture*>(context);
  if (capture->count < 16U) {
    capture->events[capture->count] = event;
    ++capture->count;
  }
}

void Reset(Capture& capture) {
  capture.count = 0;
}

bool Expect(const Capture& capture, uint32_t index,
            og::drivers::hid::EventType type, uint16_t code, int32_t value) {
  if (index >= capture.count) {
    return false;
  }
  const og::drivers::hid::Event& event = capture.events[index];
  return event.type == type && event.code == code && event.value == value;
}

int Fail(const char* message) {
  std::printf("hid probe: %s\n", message);
  return 1;
}

}  // namespace

int main() {
  og::drivers::hid::Decoder decoder;
  Capture capture{};
  decoder.SetSink(&Sink, &capture);

  const uint8_t shift_a[8] = {0x02, 0, 0x04, 0, 0, 0, 0, 0};
  if (!og::IsOk(decoder.KeyboardReport(shift_a)) || capture.count != 2U ||
      !Expect(capture, 0, og::drivers::hid::EventType::kKey, 0xE1, 1) ||
      !Expect(capture, 1, og::drivers::hid::EventType::kKey, 0x04, 1)) {
    return Fail("shift+a press");
  }

  Reset(capture);
  if (!og::IsOk(decoder.KeyboardReport(shift_a)) || capture.count != 0U) {
    return Fail("keyboard repeat suppression");
  }

  const uint8_t release_all[8] = {};
  Reset(capture);
  if (!og::IsOk(decoder.KeyboardReport(release_all)) || capture.count != 2U ||
      !Expect(capture, 0, og::drivers::hid::EventType::kKey, 0xE1, 0) ||
      !Expect(capture, 1, og::drivers::hid::EventType::kKey, 0x04, 0)) {
    return Fail("shift+a release");
  }

  const uint8_t two_keys[8] = {0, 0, 0x05, 0x06, 0, 0, 0, 0};
  Reset(capture);
  if (!og::IsOk(decoder.KeyboardReport(two_keys)) || capture.count != 2U ||
      !Expect(capture, 0, og::drivers::hid::EventType::kKey, 0x05, 1) ||
      !Expect(capture, 1, og::drivers::hid::EventType::kKey, 0x06, 1)) {
    return Fail("two key press");
  }

  const uint8_t one_key[8] = {0, 0, 0x05, 0, 0, 0, 0, 0};
  Reset(capture);
  if (!og::IsOk(decoder.KeyboardReport(one_key)) || capture.count != 1U ||
      !Expect(capture, 0, og::drivers::hid::EventType::kKey, 0x06, 0)) {
    return Fail("partial release");
  }

  if (og::IsOk(decoder.KeyboardReport(
        std::span<const uint8_t>(shift_a, 7)))) {
    return Fail("short keyboard report");
  }

  const uint8_t mouse_press[3] = {0x01, 5, 0xFD};
  Reset(capture);
  if (!og::IsOk(decoder.MouseReport(mouse_press)) || capture.count != 3U ||
      !Expect(capture, 0, og::drivers::hid::EventType::kButton, 1, 1) ||
      !Expect(capture, 1, og::drivers::hid::EventType::kAxis,
              og::drivers::hid::kAxisX, 5) ||
      !Expect(capture, 2, og::drivers::hid::EventType::kAxis,
              og::drivers::hid::kAxisY, -3)) {
    return Fail("mouse press and move");
  }

  const uint8_t mouse_still[3] = {0x01, 0, 0};
  Reset(capture);
  if (!og::IsOk(decoder.MouseReport(mouse_still)) || capture.count != 0U) {
    return Fail("mouse repeat suppression");
  }

  const uint8_t mouse_wheel[4] = {0x00, 0, 0, 1};
  Reset(capture);
  if (!og::IsOk(decoder.MouseReport(mouse_wheel)) || capture.count != 2U ||
      !Expect(capture, 0, og::drivers::hid::EventType::kButton, 1, 0) ||
      !Expect(capture, 1, og::drivers::hid::EventType::kAxis,
              og::drivers::hid::kAxisWheel, 1)) {
    return Fail("mouse release and wheel");
  }

  if (og::IsOk(decoder.MouseReport(
        std::span<const uint8_t>(mouse_press, 2)))) {
    return Fail("short mouse report");
  }

  std::printf("hid probe: OK\n");
  return 0;
}
