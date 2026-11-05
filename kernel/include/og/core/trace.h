#pragma once

// Minimal trace ring. Events are cheap records for recent kernel activity,
// not a logging replacement. The ring is safe to record from IRQ context.

#include <cstdint>

namespace og::trace {

enum class Tag : uint32_t {
  kVfs = 1,
  kBlockRead = 2,
  kBlockWrite = 3,
  kBlockFlush = 4,
  kContextSwitch = 5,
  kSpawn = 6,
  kExit = 7,
};

struct Event {
  uint64_t timestamp_ms;
  uint32_t tag;
  uint32_t value;
};

constexpr uint32_t kMaxEvents = 64;

void Record(Tag tag, uint32_t value) noexcept;

// Copies events in chronological order. Returns the number copied.
[[nodiscard]] uint32_t Snapshot(Event* out, uint32_t capacity) noexcept;

[[nodiscard]] const char* TagName(Tag tag) noexcept;
[[nodiscard]] const char* TagName(uint32_t tag) noexcept;

}  // namespace og::trace
