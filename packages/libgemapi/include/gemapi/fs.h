#pragma once

// Files, directories, and paths.

#include "gemapi/error.h"
#include "gemapi/types.h"

namespace gemapi::fs {

constexpr u64 kOpenRead = 0;
constexpr u64 kOpenWrite = 1;

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

}  // namespace gemapi::fs
