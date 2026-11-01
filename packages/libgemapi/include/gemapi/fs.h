#pragma once

// Files, directories, and paths.

#include "gemapi/error.h"
#include "gemapi/types.h"

namespace gemapi::fs {

constexpr u64 kOpenRead = 0;
constexpr u64 kOpenWrite = 1;
constexpr u64 kOpenAppend = 2;
constexpr u64 kOpenCreate = 4;
constexpr u64 kOpenTruncate = 8;

enum class Kind : u64 {
  file = 0,
  directory = 1,
  device = 2,
  other = 3,
};

struct Info {
  Kind kind;
  u64 size;
};

struct DirEntry {
  char name[64];
  Kind kind;
  u64 size;
};

// An open file. The handle is opaque and owns one descriptor; copying is
// rejected so a descriptor cannot be closed twice. Closing is idempotent.
class File {
 public:
  File() noexcept = default;
  File(const File&) = delete;
  File& operator=(const File&) = delete;
  File(File&& other) noexcept;
  File& operator=(File&& other) noexcept;
  ~File() noexcept;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] i64 descriptor() const noexcept;

  [[nodiscard]] Result<usize> read(void* buffer, usize size) noexcept;
  [[nodiscard]] Status write(const void* data, usize size) noexcept;
  [[nodiscard]] Status seek(u64 offset) noexcept;
  Status close() noexcept;

  [[nodiscard]] static Result<File> open(const char* path,
                                         u64 flags = kOpenRead) noexcept;

 private:
  explicit File(i64 descriptor) noexcept : descriptor_(descriptor) {}

  i64 descriptor_ = -1;
};

[[nodiscard]] Result<Info> stat(const char* path) noexcept;
[[nodiscard]] Status chdir(const char* path) noexcept;
[[nodiscard]] Status mkdir(const char* path) noexcept;
[[nodiscard]] Status unlink(const char* path) noexcept;
[[nodiscard]] Status rmdir(const char* path) noexcept;
[[nodiscard]] Status rename(const char* old_path, const char* new_path) noexcept;

// Reads one directory entry. Returns false at the end of the directory.
[[nodiscard]] Result<bool> list(const char* path, u64 index,
                                DirEntry& entry) noexcept;

}  // namespace gemapi::fs
