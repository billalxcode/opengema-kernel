#include "kgema/io.h"
#include "ldgem/elf.h"
#include "ldgem/linker.h"
#include "ldgem/text.h"
#include "ldgem/types.h"

namespace ldgem {

namespace {

[[nodiscard]] bool ParseHex(const char* text, u64& out) noexcept {
  if (text == nullptr) {
    return false;
  }
  if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
    text += 2;
  }
  u64 value = 0;
  u32 digits = 0;
  while (*text != '\0') {
    u32 digit = 0;
    const char character = *text;
    if (character >= '0' && character <= '9') {
      digit = static_cast<u32>(character - '0');
    } else if (character >= 'a' && character <= 'f') {
      digit = static_cast<u32>(character - 'a') + 10U;
    } else if (character >= 'A' && character <= 'F') {
      digit = static_cast<u32>(character - 'A') + 10U;
    } else {
      return false;
    }
    value = (value << 4U) | digit;
    ++digits;
    ++text;
    if (digits > 16U) {
      return false;
    }
  }
  if (digits == 0U) {
    return false;
  }
  out = value;
  return true;
}

[[nodiscard]] bool FindEnvironment(char** envp, const char* prefix,
                                    u64& out) noexcept {
  if (envp == nullptr || prefix == nullptr) {
    return false;
  }
  const u64 prefix_length = TextLength(prefix);
  for (u32 index = 0; envp[index] != nullptr; ++index) {
    if (elf::StartsWith(envp[index], prefix)) {
      return ParseHex(envp[index] + prefix_length, out);
    }
  }
  return false;
}

}  // namespace

int run(long argc, char** argv, char** envp) noexcept {
  u64 dynamic = 0;
  u64 entry = 0;
  u64 base = 0;
  if (!FindEnvironment(envp, "GEM_DYNAMIC=", dynamic) ||
      !FindEnvironment(envp, "GEM_ENTRY=", entry) ||
      !FindEnvironment(envp, "GEM_BASE=", base)) {
    WriteText("ld-gem: missing GEM_DYNAMIC, GEM_ENTRY, or GEM_BASE\n");
    return 2;
  }

  const char* name =
    (argc > 0 && argv != nullptr && argv[0] != nullptr) ? argv[0] : "main";
  Linker linker;
  if (!linker.AddMain(dynamic, base, name)) {
    WriteText("ld-gem: cannot read the main executable\n");
    return 3;
  }
  if (!linker.LoadLibraries()) {
    WriteText("ld-gem: cannot load dependencies\n");
    return 4;
  }
  if (!linker.RelocateAll()) {
    WriteText("ld-gem: relocation failed\n");
    return 5;
  }
  if (!linker.RunInitializers()) {
    WriteText("ld-gem: initializer failed\n");
    return 6;
  }

  using Entry = void (*)(long, char**, char**);
  reinterpret_cast<Entry>(entry)(argc, argv, envp);
  WriteText("ld-gem: main returned\n");
  return 7;
}

}  // namespace ldgem
