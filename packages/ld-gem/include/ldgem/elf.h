#pragma once

// ELF64 constants and byte readers shared by the image loader and the
// symbol binder.

#include "ldgem/types.h"

namespace ldgem::elf {

constexpr u16 kHeaderSize = 64;
constexpr u16 kProgramHeaderSize = 56;
constexpr u16 kTypeExec = 2;
constexpr u16 kTypeDyn = 3;
constexpr u16 kMachineAarch64 = 183;

constexpr u32 kProgramLoad = 1;
constexpr u32 kProgramDynamic = 2;
constexpr u32 kProgramInterp = 3;

// Dynamic array tags.
constexpr i64 kTagNull = 0;
constexpr i64 kTagNeeded = 1;
constexpr i64 kTagPltRelSize = 2;
constexpr i64 kTagHash = 4;
constexpr i64 kTagStrtab = 5;
constexpr i64 kTagSymtab = 6;
constexpr i64 kTagRela = 7;
constexpr i64 kTagRelaSize = 8;
constexpr i64 kTagRelaEntry = 9;
constexpr i64 kTagStrSize = 10;
constexpr i64 kTagSymEntry = 11;
constexpr i64 kTagInit = 12;
constexpr i64 kTagSoname = 14;
constexpr i64 kTagJmpRel = 23;
constexpr i64 kTagInitArray = 25;
constexpr i64 kTagInitArraySize = 27;

constexpr u32 kRelocationNone = 0;
constexpr u32 kRelocationAbs64 = 257;
constexpr u32 kRelocationGlobDat = 1025;
constexpr u32 kRelocationJumpSlot = 1026;
constexpr u32 kRelocationRelative = 1027;

constexpr u64 kSymbolEntrySize = 24;
constexpr u64 kRelaEntrySize = 24;

constexpr u32 kSymbolBindLocal = 0;
constexpr u32 kSymbolBindGlobal = 1;
constexpr u32 kSymbolBindWeak = 2;

constexpr u16 kSectionUndefined = 0;

struct ProgramHeader {
  u32 type;
  u32 flags;
  u64 offset;
  u64 virtual_address;
  u64 file_size;
  u64 memory_size;
};

[[nodiscard]] u16 Read16(const u8* data, u64 offset) noexcept;
[[nodiscard]] u32 Read32(const u8* data, u64 offset) noexcept;
[[nodiscard]] u64 Read64(const u8* data, u64 offset) noexcept;

[[nodiscard]] bool Same(const char* lhs, const char* rhs) noexcept;
[[nodiscard]] bool StartsWith(const char* text, const char* prefix) noexcept;
[[nodiscard]] u32 HashSymbolName(const char* name) noexcept;
[[nodiscard]] u64 AlignUp(u64 value, u64 align) noexcept;

}  // namespace ldgem::elf
