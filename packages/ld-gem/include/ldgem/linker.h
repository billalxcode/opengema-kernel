#pragma once

// Symbol scope, relocation, and initializer handling for loaded objects.

#include "ldgem/types.h"

namespace ldgem {

constexpr u32 kMaxObjects = 8;
constexpr u32 kMaxName = 64;

struct Object {
  char name[kMaxName];
  u64 base;
  bool is_main;

  u64 dynamic;
  u64 hash;
  u64 string_table;
  u64 symbol_table;
  u64 rela;
  u64 rela_size;
  u64 jump_rela;
  u64 jump_rela_size;
  u64 init;
  u64 init_array;
  u64 init_array_size;
};

class Linker {
 public:
  // dynamic is the address of the main executable's dynamic array. base is
  // zero for ET_EXEC images.
  [[nodiscard]] bool AddMain(u64 dynamic, const char* name) noexcept;

  // Loads every DT_NEEDED entry recursively from the search path.
  [[nodiscard]] bool LoadLibraries() noexcept;

  [[nodiscard]] bool RelocateAll() noexcept;
  [[nodiscard]] bool RunInitializers() noexcept;

  [[nodiscard]] u32 count() const noexcept { return count_; }
  [[nodiscard]] const Object& object(u32 index) const noexcept {
    return objects_[index];
  }

 private:
  Object objects_[kMaxObjects]{};
  u32 count_ = 0;

  [[nodiscard]] bool AddObject(const char* name, u64 base, u64 dynamic,
                               bool is_main) noexcept;
  [[nodiscard]] bool ParseDynamic(Object& object) noexcept;
  [[nodiscard]] bool LoadNeeded(const Object& object) noexcept;
  [[nodiscard]] bool AddLibrary(const char* name) noexcept;
  [[nodiscard]] bool HasObject(const char* name) const noexcept;
  [[nodiscard]] u64 Lookup(const char* name) noexcept;
  [[nodiscard]] bool Relocate(Object& object) noexcept;
  [[nodiscard]] bool RelocateRange(Object& object, u64 address,
                                   u64 size) noexcept;
  [[nodiscard]] bool RunInitArray(const Object& object) noexcept;
};

}  // namespace ldgem
