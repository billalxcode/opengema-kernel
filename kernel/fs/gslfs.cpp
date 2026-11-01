#include "og/fs/gslfs.h"

#include "memory.h"
#include "og/core/log.h"

namespace og::fs::gslfs {

namespace {

constexpr uint32_t kSectorSize = 512;
constexpr uint32_t kNameMax = 64;
constexpr uint32_t kSuperblockSize = 60;
constexpr uint32_t kSuperblockCrcSize = 56;
constexpr uint32_t kNodeSize = 56;
constexpr uint32_t kNodeCrcSize = 48;
constexpr uint32_t kNodeTypeFile = 0;
constexpr uint32_t kNodeTypeDirectory = 1;

struct GslfsNode {
  Node node;
  char name[kNameMax];
  uint32_t index;
  uint32_t parent;
  uint64_t data_offset;
  uint32_t mode;
  uint32_t data_checksum;
};

BlockDevice g_device{};
GslfsNode g_nodes[kMaxNodes];
uint32_t g_count = 0;
bool g_ready = false;

[[nodiscard]] uint32_t Read32(const uint8_t* data) noexcept {
  return static_cast<uint32_t>(data[0]) |
         (static_cast<uint32_t>(data[1]) << 8U) |
         (static_cast<uint32_t>(data[2]) << 16U) |
         (static_cast<uint32_t>(data[3]) << 24U);
}

[[nodiscard]] uint64_t Read64(const uint8_t* data) noexcept {
  uint64_t value = 0;
  for (uint32_t index = 0; index < 8U; ++index) {
    value |= static_cast<uint64_t>(data[index]) << (index * 8U);
  }
  return value;
}

[[nodiscard]] uint32_t Crc32Update(uint32_t crc, const uint8_t* data,
                                   uint64_t size) noexcept {
  for (uint64_t index = 0; index < size; ++index) {
    crc ^= data[index];
    for (uint32_t bit = 0; bit < 8U; ++bit) {
      const uint32_t mask = 0U - (crc & 1U);
      crc = (crc >> 1U) ^ (0xEDB88320U & mask);
    }
  }
  return crc;
}

[[nodiscard]] uint32_t Crc32(const uint8_t* data, uint64_t size) noexcept {
  return ~Crc32Update(0xFFFFFFFFU, data, size);
}

[[nodiscard]] Status ReadBytes(const BlockDevice& device, uint64_t offset,
                               std::span<uint8_t> out) noexcept {
  if (out.empty()) {
    return Status::kOk;
  }
  if (device.read == nullptr) {
    return Status::kInvalidArg;
  }
  uint8_t sector[kSectorSize];
  uint64_t done = 0;
  while (done < out.size()) {
    const uint64_t position = offset + done;
    const uint64_t sector_index = position / kSectorSize;
    const uint32_t within = static_cast<uint32_t>(position % kSectorSize);
    const Status status = device.read(
      device.context, sector_index + device.sector_offset,
      std::span<uint8_t>(sector, kSectorSize));
    if (IsErr(status)) {
      return status;
    }
    uint64_t chunk = kSectorSize - within;
    if (chunk > (out.size() - done)) {
      chunk = out.size() - done;
    }
    mem::Copy(out.data() + done, sector + within, chunk);
    done += chunk;
  }
  return Status::kOk;
}

[[nodiscard]] Status ReadName(const BlockDevice& device, uint64_t name_base,
                              uint64_t name_table_size, uint32_t offset,
                              char* out) noexcept {
  if (out == nullptr || offset >= name_table_size) {
    return Status::kInvalidArg;
  }
  uint64_t available = name_table_size - offset;
  if (available > kNameMax) {
    available = kNameMax;
  }
  uint8_t buffer[kNameMax];
  const Status status = ReadBytes(
    device, name_base + offset,
    std::span<uint8_t>(buffer, static_cast<std::size_t>(available)));
  if (IsErr(status)) {
    return status;
  }
  uint64_t length = 0;
  while (length < available && buffer[length] != '\0') {
    if (buffer[length] == '/') {
      return Status::kInvalidArg;
    }
    ++length;
  }
  if (length >= available || length >= kNameMax) {
    return Status::kInvalidArg;
  }
  mem::Copy(out, buffer, length + 1U);
  return Status::kOk;
}

[[nodiscard]] Status FileRead(Node* node, uint64_t offset,
                              std::span<uint8_t> buffer,
                              uint64_t& out_read) noexcept {
  out_read = 0;
  if (!g_ready || node == nullptr || node->type != NodeType::kFile ||
      buffer.empty()) {
    return Status::kInvalidArg;
  }
  if (offset >= node->size) {
    return Status::kOk;
  }
  uint64_t available = node->size - offset;
  if (available > buffer.size()) {
    available = buffer.size();
  }
  auto* file = static_cast<GslfsNode*>(node->fs_data);
  if (file == nullptr) {
    return Status::kNotInit;
  }
  const Status status = ReadBytes(
    g_device, file->data_offset + offset,
    std::span<uint8_t>(buffer.data(), static_cast<std::size_t>(available)));
  if (IsErr(status)) {
    return status;
  }
  out_read = available;
  return Status::kOk;
}

[[nodiscard]] Status FileWrite(Node* /*node*/, uint64_t /*offset*/,
                               std::span<const uint8_t> /*buffer*/,
                               uint64_t& out_written) noexcept {
  out_written = 0;
  return Status::kUnsupported;
}

[[nodiscard]] Status DirLookup(Node* directory, const char* name,
                               Node** out) noexcept {
  if (!g_ready || directory == nullptr || name == nullptr || out == nullptr) {
    return Status::kInvalidArg;
  }
  *out = nullptr;
  auto* self = static_cast<GslfsNode*>(directory->fs_data);
  if (self == nullptr) {
    return Status::kNotInit;
  }
  for (uint32_t index = 0; index < g_count; ++index) {
    if (g_nodes[index].parent == self->index &&
        mem::Same(g_nodes[index].name, name)) {
      *out = &g_nodes[index].node;
      return Status::kOk;
    }
  }
  return Status::kNotFound;
}

[[nodiscard]] Status DirIterate(Node* directory, uint32_t index,
                                DirEntry& entry, bool& found) noexcept {
  found = false;
  if (!g_ready || directory == nullptr) {
    return Status::kInvalidArg;
  }
  auto* self = static_cast<GslfsNode*>(directory->fs_data);
  if (self == nullptr) {
    return Status::kNotInit;
  }
  uint32_t seen = 0;
  for (uint32_t node_index = 0; node_index < g_count; ++node_index) {
    if (g_nodes[node_index].parent != self->index) {
      continue;
    }
    if (seen == index) {
      const GslfsNode& node = g_nodes[node_index];
      mem::Zero(&entry, sizeof(entry));
      const std::size_t length = mem::Length(node.name);
      if (length >= sizeof(entry.name)) {
        return Status::kInvalidArg;
      }
      mem::Copy(entry.name, node.name, length + 1U);
      entry.type = node.node.type;
      entry.size = node.node.size;
      found = true;
      return Status::kOk;
    }
    ++seen;
  }
  return Status::kOk;
}

const NodeOps kFileOps{nullptr, &FileRead, &FileWrite, nullptr};
const NodeOps kDirectoryOps{&DirLookup, nullptr, nullptr, &DirIterate};

[[nodiscard]] bool VerifyData(const GslfsNode& node,
                              uint32_t expected) noexcept {
  if (node.node.size == 0U) {
    return expected == 0U;
  }
  uint32_t crc = 0xFFFFFFFFU;
  uint64_t offset = node.data_offset;
  uint64_t remaining = node.node.size;
  uint8_t buffer[kSectorSize];
  while (remaining > 0U) {
    uint64_t chunk = remaining;
    if (chunk > kSectorSize) {
      chunk = kSectorSize;
    }
    const Status status = ReadBytes(
      g_device, offset, std::span<uint8_t>(buffer, static_cast<std::size_t>(chunk)));
    if (IsErr(status)) {
      return false;
    }
    crc = Crc32Update(crc, buffer, chunk);
    offset += chunk;
    remaining -= chunk;
  }
  return ~crc == expected;
}

}  // namespace

Result<Node*> Mount(const BlockDevice& device) noexcept {
  if (g_ready) {
    return Result<Node*>::Err(Status::kExists);
  }
  if (device.read == nullptr || device.sector_count == 0U) {
    return Result<Node*>::Err(Status::kInvalidArg);
  }

  uint8_t superblock[kSuperblockSize];
  Status status = ReadBytes(
    device, 0, std::span<uint8_t>(superblock, sizeof(superblock)));
  if (IsErr(status)) {
    return Result<Node*>::Err(status);
  }
  if (!mem::Equal(superblock, kMagic, sizeof(kMagic))) {
    return Result<Node*>::Err(Status::kNoDevice);
  }

  const uint32_t version = Read32(superblock + 8);
  const uint32_t block_size = Read32(superblock + 12);
  const uint64_t block_count = Read64(superblock + 16);
  const uint32_t node_count = Read32(superblock + 24);
  const uint32_t root = Read32(superblock + 28);
  const uint64_t node_offset = Read64(superblock + 32);
  const uint64_t name_offset = Read64(superblock + 40);
  const uint64_t data_offset = Read64(superblock + 48);
  const uint32_t checksum = Read32(superblock + 56);

  if (version != kVersion || block_size != kBlockSize) {
    return Result<Node*>::Err(Status::kUnsupported);
  }
  if (Crc32(superblock, kSuperblockCrcSize) != checksum) {
    return Result<Node*>::Err(Status::kIoError);
  }
  if (block_count == 0U ||
      block_count > ((~0ULL) / static_cast<uint64_t>(kBlockSize))) {
    return Result<Node*>::Err(Status::kInvalidArg);
  }
  const uint64_t image_bytes = block_count * kBlockSize;
  if (device.sector_count > ((~0ULL) / kSectorSize) ||
      image_bytes > (device.sector_count * kSectorSize)) {
    return Result<Node*>::Err(Status::kInvalidArg);
  }
  if (node_count == 0U || node_count > kMaxNodes || root >= node_count) {
    return Result<Node*>::Err(Status::kInvalidArg);
  }
  if (node_offset % kBlockSize != 0U ||
      node_offset > image_bytes ||
      (static_cast<uint64_t>(node_count) * kNodeSize) >
        (image_bytes - node_offset)) {
    return Result<Node*>::Err(Status::kInvalidArg);
  }
  if (name_offset >= data_offset || data_offset > image_bytes ||
      data_offset % kBlockSize != 0U) {
    return Result<Node*>::Err(Status::kInvalidArg);
  }
  const uint64_t name_table_size = data_offset - name_offset;

  mem::Zero(g_nodes, sizeof(g_nodes));
  g_count = node_count;
  g_device = device;

  for (uint32_t index = 0; index < node_count; ++index) {
    uint8_t raw[kNodeSize];
    status = ReadBytes(
      device, node_offset + (static_cast<uint64_t>(index) * kNodeSize),
      std::span<uint8_t>(raw, sizeof(raw)));
    if (IsErr(status)) {
      return Result<Node*>::Err(status);
    }
    if (Crc32(raw, kNodeCrcSize) != Read32(raw + 48U)) {
      return Result<Node*>::Err(Status::kIoError);
    }

    const uint32_t type = Read32(raw);
    const uint32_t parent = Read32(raw + 16U);
    const uint32_t name_entry = Read32(raw + 20U);
    const uint64_t size = Read64(raw + 24U);
    const uint64_t data_block = Read64(raw + 32U);
    const uint64_t blocks = Read64(raw + 40U);
    const uint32_t data_checksum = Read32(raw + 52U);

    if (type != kNodeTypeFile && type != kNodeTypeDirectory) {
      return Result<Node*>::Err(Status::kInvalidArg);
    }
    if (parent >= node_count || (index == root && parent != root)) {
      return Result<Node*>::Err(Status::kInvalidArg);
    }

    GslfsNode& node = g_nodes[index];
    node.index = index;
    node.parent = parent;
    node.mode = Read32(raw + 4U);
    node.data_checksum = data_checksum;
    node.node.type = type == kNodeTypeFile ? NodeType::kFile
                                           : NodeType::kDirectory;
    node.node.size = size;

    if (index == root) {
      node.node.name = "/";
      if (type != kNodeTypeDirectory) {
        return Result<Node*>::Err(Status::kInvalidArg);
      }
    } else {
      status = ReadName(device, name_offset, name_table_size, name_entry,
                        node.name);
      if (IsErr(status) || node.name[0] == '\0') {
        return Result<Node*>::Err(
          IsErr(status) ? status : Status::kInvalidArg);
      }
      node.node.name = node.name;
    }

    if (type == kNodeTypeFile) {
      node.node.ops = &kFileOps;
      if (size == 0U) {
        if (blocks != 0U) {
          return Result<Node*>::Err(Status::kInvalidArg);
        }
        node.data_offset = 0;
      } else {
        if (data_block < (data_offset / kBlockSize) ||
            blocks != ((size + kBlockSize - 1U) / kBlockSize) ||
            blocks > (block_count - data_block)) {
          return Result<Node*>::Err(Status::kInvalidArg);
        }
        node.data_offset = data_block * kBlockSize;
      }
    } else {
      node.node.ops = &kDirectoryOps;
      if (size != 0U || blocks != 0U) {
        return Result<Node*>::Err(Status::kInvalidArg);
      }
      node.data_offset = 0;
    }
    node.node.fs_data = &node;
  }

  for (uint32_t index = 0; index < node_count; ++index) {
    if (index == root) {
      continue;
    }
    const uint32_t parent = g_nodes[index].parent;
    if (g_nodes[parent].node.type != NodeType::kDirectory) {
      return Result<Node*>::Err(Status::kInvalidArg);
    }
    uint32_t cursor = index;
    bool reached_root = false;
    for (uint32_t depth = 0; depth < node_count; ++depth) {
      if (cursor == root) {
        reached_root = true;
        break;
      }
      cursor = g_nodes[cursor].parent;
    }
    if (!reached_root) {
      return Result<Node*>::Err(Status::kInvalidArg);
    }
  }

  for (uint32_t index = 0; index < node_count; ++index) {
    if (g_nodes[index].node.type != NodeType::kFile) {
      continue;
    }
    if (!VerifyData(g_nodes[index], g_nodes[index].data_checksum)) {
      return Result<Node*>::Err(Status::kIoError);
    }
  }

  g_ready = true;
  LogDec(LogLevel::kInfo, "fs: gslfs nodes",
         static_cast<uint64_t>(g_count));
  return Result<Node*>::Ok(&g_nodes[root].node);
}

uint32_t NodeCount() noexcept { return g_count; }

}  // namespace og::fs::gslfs
