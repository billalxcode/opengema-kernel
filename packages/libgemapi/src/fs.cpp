#include "gemapi/fs.h"

#include "kgema/file.h"

namespace gemapi::fs {

namespace {

[[nodiscard]] Kind ConvertKind(u64 type) noexcept {
  switch (type) {
    case 0:
      return Kind::file;
    case 1:
      return Kind::directory;
    case 2:
      return Kind::device;
    default:
      return Kind::other;
  }
}

}  // namespace

File::File(File&& other) noexcept : descriptor_(other.descriptor_) {
  other.descriptor_ = -1;
}

File& File::operator=(File&& other) noexcept {
  if (this != &other) {
    (void)close();
    descriptor_ = other.descriptor_;
    other.descriptor_ = -1;
  }
  return *this;
}

File::~File() noexcept { (void)close(); }

bool File::valid() const noexcept { return descriptor_ >= 0; }

i64 File::descriptor() const noexcept { return descriptor_; }

Result<usize> File::read(void* buffer, usize size) noexcept {
  if (!valid()) {
    return Status::invalid_argument;
  }
  if (buffer == nullptr) {
    return Status::invalid_argument;
  }
  if (size == 0U) {
    return usize{0};
  }
  const i64 count = kgema::read(descriptor_, buffer, size);
  if (count < 0) {
    return Status::io;
  }
  return static_cast<usize>(count);
}

Status File::write(const void* data, usize size) noexcept {
  if (!valid()) {
    return Status::invalid_argument;
  }
  if (data == nullptr) {
    return Status::invalid_argument;
  }
  const auto* bytes = static_cast<const char*>(data);
  usize written = 0;
  while (written < size) {
    const i64 count = kgema::write(descriptor_, bytes + written, size - written);
    if (count <= 0) {
      return Status::io;
    }
    written += static_cast<usize>(count);
  }
  return Status::ok;
}

Status File::seek(u64 offset) noexcept {
  if (!valid()) {
    return Status::invalid_argument;
  }
  return kgema::seek(descriptor_, offset) ? Status::ok : Status::io;
}

Status File::close() noexcept {
  if (!valid()) {
    return Status::ok;
  }
  const bool closed = kgema::close(descriptor_);
  descriptor_ = -1;
  return closed ? Status::ok : Status::io;
}

Result<File> File::open(const char* path, u64 flags) noexcept {
  if (path == nullptr) {
    return Status::invalid_argument;
  }
  const i64 descriptor = kgema::open(path, flags);
  if (descriptor < 0) {
    return Status::not_found;
  }
  return File(descriptor);
}

Result<Info> stat(const char* path) noexcept {
  if (path == nullptr) {
    return Status::invalid_argument;
  }
  kgema::Stat raw{};
  if (!kgema::stat(path, raw)) {
    return Status::not_found;
  }
  Info info{};
  info.kind = ConvertKind(raw.type);
  info.size = raw.size;
  return info;
}

Status chdir(const char* path) noexcept {
  if (path == nullptr) {
    return Status::invalid_argument;
  }
  return kgema::chdir(path) ? Status::ok : Status::not_found;
}

Result<bool> list(const char* path, u64 index, DirEntry& entry) noexcept {
  if (path == nullptr) {
    return Status::invalid_argument;
  }
  kgema::DirEntry raw{};
  const i64 result = kgema::list(path, index, raw);
  if (result < 0) {
    return Status::io;
  }
  if (result == 0) {
    return false;
  }
  usize length = 0;
  while (length + 1U < sizeof(entry.name) && raw.name[length] != '\0') {
    entry.name[length] = raw.name[length];
    ++length;
  }
  entry.name[length] = '\0';
  entry.kind = ConvertKind(raw.type);
  entry.size = raw.size;
  return true;
}

}  // namespace gemapi::fs
