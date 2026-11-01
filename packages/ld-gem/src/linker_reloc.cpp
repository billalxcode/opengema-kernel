#include "ldgem/linker.h"

#include "ldgem/elf.h"
#include "ldgem/text.h"

namespace ldgem {

bool Linker::RelocateAll() noexcept {
  for (u32 index = 0; index < count_; ++index) {
    if (!Relocate(objects_[index])) {
      return false;
    }
  }
  return true;
}

bool Linker::Relocate(Object& object) noexcept {
  if (object.rela != 0U && object.rela_size != 0U) {
    if (!RelocateRange(object, object.rela, object.rela_size)) {
      return false;
    }
  }
  if (object.jump_rela != 0U && object.jump_rela_size != 0U) {
    if (!RelocateRange(object, object.jump_rela, object.jump_rela_size)) {
      return false;
    }
  }
  return true;
}

bool Linker::RelocateRange(Object& object, u64 address, u64 size) noexcept {
  const auto* table = reinterpret_cast<const u8*>(address);
  for (u64 offset = 0; (offset + elf::kRelaEntrySize) <= size;
       offset += elf::kRelaEntrySize) {
    const u64 relocation_offset = elf::Read64(table, offset);
    const u64 info = elf::Read64(table, offset + 8U);
    const i64 addend = static_cast<i64>(elf::Read64(table, offset + 16U));
    const u32 type = static_cast<u32>(info & 0xFFFFFFFFULL);
    const u32 symbol_index = static_cast<u32>(info >> 32U);
    auto* where = reinterpret_cast<u64*>(object.base + relocation_offset);

    if (type == elf::kRelocationNone) {
      continue;
    }
    if (type == elf::kRelocationRelative) {
      *where = object.base + static_cast<u64>(addend);
      continue;
    }
    if (type != elf::kRelocationAbs64 && type != elf::kRelocationGlobDat &&
        type != elf::kRelocationJumpSlot) {
      WriteText("ld-gem: unsupported relocation\n");
      return false;
    }

    const auto* symbol =
      reinterpret_cast<const u8*>(object.symbol_table) +
      (static_cast<u64>(symbol_index) * elf::kSymbolEntrySize);
    const auto* name = reinterpret_cast<const char*>(
      object.string_table + elf::Read32(symbol, 0));
    const u64 target = Lookup(name);
    if (target == 0U) {
      ReportSymbol("ld-gem: unresolved: ", name);
      return false;
    }
    *where = target + static_cast<u64>(addend);
  }
  return true;
}

bool Linker::RunInitializers() noexcept {
  for (u32 index = count_; index > 0U; --index) {
    const Object& object = objects_[index - 1U];
    if (object.init != 0U) {
      reinterpret_cast<void (*)()>(object.init)();
    }
    if (!RunInitArray(object)) {
      return false;
    }
  }
  return true;
}

bool Linker::RunInitArray(const Object& object) noexcept {
  if (object.init_array == 0U || object.init_array_size == 0U) {
    return true;
  }
  const u64 count = object.init_array_size / sizeof(u64);
  const auto* entries = reinterpret_cast<const u64*>(object.init_array);
  for (u64 index = 0; index < count; ++index) {
    if (entries[index] != 0U) {
      reinterpret_cast<void (*)()>(entries[index])();
    }
  }
  return true;
}

}  // namespace ldgem
