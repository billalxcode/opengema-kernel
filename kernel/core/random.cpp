#include "og/core/random.h"

#include "og/arch/registers.h"

namespace og::core {

namespace {

uint64_t g_random_counter = 0;

}  // namespace

uint64_t Random64() noexcept {
  const uint64_t sequence =
    __atomic_add_fetch(&g_random_counter, 1U, __ATOMIC_RELAXED);
  uint64_t value = arch::aarch64::ReadCntpctEl0() ^
                   (sequence * 0x9E3779B97F4A7C15ULL);
  value ^= value >> 30U;
  value *= 0xBF58476D1CE4E5B9ULL;
  value ^= value >> 27U;
  value *= 0x94D049BB133111EBULL;
  value ^= value >> 31U;
  return value;
}

}  // namespace og::core
