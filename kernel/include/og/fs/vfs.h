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
  Status (*lookup)(Node* directory, const char* name, Node** out) noexcept;
  Status (*read)(Node* node, uint64_t offset, std::span<uint8_t> buffer,
                 uint64_t& out_read) noexcept;
  Status (*write)(Node* node, uint64_t offset, std::span<const uint8_t> buffer,
                  uint64_t& out_written) noexcept;
  Status (*iterate)(Node* directory, uint32_t index, DirEntry& entry,
                    bool& found) noexcept;
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

constexpr uint32_t kMaxMounts = 8;
constexpr uint32_t kMaxFiles = 16;
constexpr uint32_t kMaxPath = 256;

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

}  // namespace og::fs
