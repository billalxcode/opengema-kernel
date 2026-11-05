#pragma once

// File helpers. Paths are NUL terminated strings in the caller address space.

#include "kgema/types.h"

namespace kgema {

constexpr u64 kOpenRead = 0;
constexpr u64 kOpenWrite = 1;
constexpr u64 kOpenAppend = 2;
constexpr u64 kOpenCreate = 4;
constexpr u64 kOpenTruncate = 8;

struct Stat {
  u64 type;
  u64 size;
  u64 mode;
  u64 uid;
  u64 gid;
  u64 mtime;
};

struct DirEntry {
  char name[64];
  u64 type;
  u64 size;
};

// Returns a descriptor, or -1 on error.
[[nodiscard]] i64 open(const char* path, u64 flags = kOpenRead) noexcept;
[[nodiscard]] bool close(i64 descriptor) noexcept;
[[nodiscard]] i64 read(i64 descriptor, void* buffer, usize size) noexcept;
[[nodiscard]] i64 write(i64 descriptor, const void* data, usize size) noexcept;
[[nodiscard]] bool seek(i64 descriptor, u64 offset) noexcept;
[[nodiscard]] bool stat(const char* path, Stat& result) noexcept;
[[nodiscard]] bool chdir(const char* path) noexcept;
[[nodiscard]] bool mkdir(const char* path) noexcept;
[[nodiscard]] bool unlink(const char* path) noexcept;
[[nodiscard]] bool rmdir(const char* path) noexcept;
[[nodiscard]] bool rename(const char* old_path, const char* new_path) noexcept;
[[nodiscard]] bool chmod(const char* path, u64 mode) noexcept;
[[nodiscard]] bool chown(const char* path, u64 uid, u64 gid) noexcept;
[[nodiscard]] bool mount(const char* type, const char* target) noexcept;
[[nodiscard]] bool umount(const char* target) noexcept;

// Returns 1 when an entry is written, 0 at the end, -1 on error.
[[nodiscard]] i64 list(const char* path, u64 index, DirEntry& entry) noexcept;

}  // namespace kgema
