#pragma once

// Canonical owner of the virtual filesystem: node model, mount table,
// path resolution, and the file descriptor table.

#include <cstdint>
#include <span>

#include "og/core/status.h"

namespace og::fs {

enum class NodeType : uint32_t {
  kFile = 0,
  kDirectory = 1,
  kDevice = 2,
};

struct Node;

struct DirEntry {
  char name[64];
  NodeType type;
  uint64_t size;
};

struct NodeOps {
  Status (*lookup)(Node* directory, const char* name, Node** out) noexcept =
    nullptr;
  Status (*read)(Node* node, uint64_t offset, std::span<uint8_t> buffer,
                 uint64_t& out_read) noexcept = nullptr;
  Status (*write)(Node* node, uint64_t offset,
                  std::span<const uint8_t> buffer,
                  uint64_t& out_written) noexcept = nullptr;
  Status (*iterate)(Node* directory, uint32_t index, DirEntry& entry,
                    bool& found) noexcept = nullptr;
  Status (*create)(Node* directory, const char* name, NodeType type,
                   Node** out) noexcept = nullptr;
  Status (*remove)(Node* directory, Node* node) noexcept = nullptr;
  Status (*rename)(Node* directory, Node* node, Node* new_directory,
                   const char* new_name) noexcept = nullptr;
  Status (*truncate)(Node* node, uint64_t size) noexcept = nullptr;
};

struct Node {
  const char* name;
  NodeType type;
  uint64_t size;
  const NodeOps* ops;
  void* fs_data;
  void* device_data;
};

struct Stat {
  NodeType type;
  uint64_t size;
};

struct FileEntry {
  Node* node;
  uint64_t offset;
  uint32_t flags;
  bool in_use;
};

constexpr uint32_t kMaxMounts = 16;
constexpr uint32_t kMaxFiles = 16;
constexpr uint32_t kMaxPath = 256;
// Open flags. Write implies read access until a separate mode is needed.
constexpr uint32_t kOpenRead = 0;
constexpr uint32_t kOpenWrite = 1U << 0;
constexpr uint32_t kOpenAppend = 1U << 1;
constexpr uint32_t kOpenCreate = 1U << 2;
constexpr uint32_t kOpenTruncate = 1U << 3;
// 0 stdin, 1 stdout, 2 stderr are reserved. Files start at 3.
constexpr uint32_t kFirstFileDescriptor = 3;

// One file table per process. The kernel keeps its own table for boot
// probes, so no file descriptor is global any more.
struct FileTable {
  FileEntry entries[kMaxFiles];
};

[[nodiscard]] Status FsInit() noexcept;
[[nodiscard]] Status FileTableInit(FileTable& table) noexcept;

// Mounts root at path. Longest matching mount wins during resolution.
[[nodiscard]] Status FsMount(const char* path, Node* root) noexcept;

[[nodiscard]] Result<uint32_t> FsOpen(FileTable& table, const char* path,
                                      uint32_t flags = 0) noexcept;
[[nodiscard]] Status FsRead(FileTable& table, uint32_t descriptor,
                            std::span<uint8_t> buffer, uint64_t& out_read) noexcept;
[[nodiscard]] Status FsWrite(FileTable& table, uint32_t descriptor,
                             std::span<const uint8_t> buffer,
                             uint64_t& out_written) noexcept;
[[nodiscard]] Status FsSeek(FileTable& table, uint32_t descriptor,
                            uint64_t offset) noexcept;
[[nodiscard]] Status FsClose(FileTable& table, uint32_t descriptor) noexcept;

[[nodiscard]] Result<Stat> FsStat(const char* path) noexcept;
[[nodiscard]] Result<Node*> FsResolve(const char* path) noexcept;
[[nodiscard]] Status FsList(const char* path, uint32_t index, DirEntry& entry,
                            bool& found) noexcept;

[[nodiscard]] Result<Node*> FsCreate(const char* path,
                                     NodeType type) noexcept;
[[nodiscard]] Status FsRemove(const char* path, NodeType type) noexcept;
[[nodiscard]] Status FsRename(const char* old_path,
                              const char* new_path) noexcept;
[[nodiscard]] Status FsTruncate(const char* path, uint64_t size) noexcept;

}  // namespace og::fs
