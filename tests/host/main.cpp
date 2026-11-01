// Host unit tests for pure libs/core code. No kernel, no hardware.

#include <cstdio>

#include "bit.h"
#include "format.h"
#include "memory.h"

namespace {

int g_failures = 0;

void Check(bool passed, const char* expression, int line) {
  if (passed) {
    return;
  }
  std::printf("FAIL line %d: %s\n", line, expression);
  ++g_failures;
}

#define CHECK(expr) Check((expr), #expr, __LINE__)

void TestMemory() {
  char buffer[16];
  og::mem::Fill(buffer, 'A', sizeof(buffer));
  CHECK(buffer[0] == 'A');
  CHECK(buffer[15] == 'A');

  og::mem::Zero(buffer, sizeof(buffer));
  CHECK(buffer[0] == 0);
  CHECK(buffer[15] == 0);

  CHECK(og::mem::Length("hello") == 5);
  CHECK(og::mem::Length("") == 0);
  CHECK(og::mem::Same("abc", "abc"));
  CHECK(!og::mem::Same("abc", "abd"));

  CHECK(memcmp("abc", "abc", 3) == 0);
  CHECK(memcmp("abc", "abd", 3) < 0);
  CHECK(memcmp("abd", "abc", 3) > 0);

  char text[8] = "abcdefg";
  og::mem::Move(text + 1, text, 6);
  CHECK(text[0] == 'a');
  CHECK(text[1] == 'a');
  CHECK(text[6] == 'f');
  CHECK(text[7] == '\0');
}

void TestFormat() {
  char line[64];

  og::fmt::LineBuilder decimal(line, sizeof(line));
  decimal.Text("value").Text(": ").Dec(12345);
  CHECK(og::mem::Same(line, "value: 12345"));

  og::fmt::LineBuilder zero(line, sizeof(line));
  zero.Dec(0);
  CHECK(og::mem::Same(line, "0"));

  og::fmt::LineBuilder hex(line, sizeof(line));
  hex.Hex(0x1A2B);
  CHECK(og::mem::Same(line, "0x0000000000001a2b"));

  og::fmt::LineBuilder full(line, sizeof(line));
  full.Hex(0xFFFFFFFFFFFFFFFFULL);
  CHECK(og::mem::Same(line, "0xffffffffffffffff"));

  char small[4];
  og::fmt::LineBuilder tiny(small, sizeof(small));
  tiny.Text("abcdef");
  CHECK(og::mem::Length(small) == 3);
  CHECK(og::mem::Same(small, "abc"));
}

void TestBits() {
  static_assert(og::bits::Mask<uint32_t>(4) == 0xFU);
  static_assert(og::bits::Mask<uint32_t>(0) == 0U);
  static_assert(og::bits::Mask<uint32_t>(32) == 0xFFFFFFFFU);
  static_assert(og::bits::Extract<4, 4>(0xABU) == 0xAU);
  static_assert(og::bits::Insert<4, 4>(0x0FU, 0xAU) == 0xAFU);
  static_assert(og::bits::Bit<uint32_t>(3) == 8U);
  static_assert(og::bits::AlignUp<uint64_t>(0x1001, 0x1000) == 0x2000);
  static_assert(og::bits::AlignDown<uint64_t>(0x1FFF, 0x1000) == 0x1000);
  static_assert(og::bits::IsAligned<uint64_t>(0x2000, 0x1000));
  static_assert(!og::bits::IsAligned<uint64_t>(0x2001, 0x1000));

  CHECK((og::bits::Extract<0, 8>(0x1234U) == 0x34U));
  CHECK(og::bits::AlignUp<uint64_t>(1, 4096) == 4096);
}

}  // namespace

int main() {
  TestMemory();
  TestFormat();
  TestBits();

  if (g_failures != 0) {
    std::printf("host tests: %d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("host tests: all passed\n");
  return 0;
}
