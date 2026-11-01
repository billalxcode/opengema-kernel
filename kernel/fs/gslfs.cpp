#include "og/fs/gslfs.h"

#include "endian.h"
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
  // Kept so permission enforcement can use the on-disk mode later.
  uint32_t mode;
  uint32_t data_checksum;
};

BlockDevice g_device{};
GslfsNode g_nodes[kMaxNodes];
uint32_t g_count = 0;
bool g_ready = false;

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

struct Superblock {
  uint64_t block_count;
  uint64_t node_offset;
  uint64_t name_offset;
  uint64_t data_offset;
  uint64_t name_table_size;
  uint64_t image_bytes;
  uint32_t node_count;
  uint32_t root;
};

[[nodiscard]] Result<Superblock> ReadSuperblock(
  const BlockDevice& device) noexcept {
  uint8_t raw[kSuperblockSize];
  Status status = ReadBytes(
    device, 0, std::span<uint8_t>(raw, sizeof(raw)));
  if (IsErr(status)) {
    return Result<Superblock>::Err(status);
  }
  if (!mem::Equal(raw, kMagic, sizeof(kMagic))) {
    return Result<Superblock>::Err(Status::kNoDevice);
  }

  const uint32_t version = endian::ReadLe32(raw + 8);
  const uint32_t block_size = endian::ReadLe32(raw + 12);
  const uint64_t block_count = endian::ReadLe64(raw + 16);
  const uint32_t node_count = endian::ReadLe32(raw + 24);
  const uint32_t root = endian::ReadLe32(raw + 28);
  const uint64_t node_offset = endian::ReadLe64(raw + 32);
  const uint64_t name_offset = endian::ReadLe64(raw + 40);
  const uint64_t data_offset = endian::ReadLe64(raw + 48);
  const uint32_t checksum = endian::ReadLe32(raw + 56);

  if (version != kVersion || block_size != kBlockSize) {
    return Result<Superblock>::Err(Status::kUnsupported);
  }
  if (Crc32(raw, kSuperblockCrcSize) != checksum) {
    return Result<Superblock>::Err(Status::kIoError);
  }
  if (block_count == 0U ||
      block_count > ((~0ULL) / static_cast<uint64_t>(kBlockSize))) {
    return Result<Superblock>::Err(Status::kInvalidArg);
  }
  const uint64_t image_bytes = block_count * kBlockSize;
  if (device.sector_count > ((~0ULL) / kSectorSize) ||
      image_bytes > (device.sector_count * kSectorSize)) {
    return Result<Superblock>::Err(Status::kInvalidArg);
  }
  if (node_count == 0U || node_count > kMaxNodes || root >= node_count) {
    return Result<Superblock>::Err(Status::kInvalidArg);
  }
  if (node_offset % kBlockSize != 0U ||
      node_offset > image_bytes ||
      (static_cast<uint64_t>(node_count) * kNodeSize) >
        (image_bytes - node_offset)) {
    return Result<Superblock>::Err(Status::kInvalidArg);
  }
  if (name_offset >= data_offset || data_offset > image_bytes ||
      data_offset % kBlockSize != 0U) {
    return Result<Superblock>::Err(Status::kInvalidArg);
  }

  Superblock super{};
  super.block_count = block_count;
  super.node_offset = node_offset;
  super.name_offset = name_offset;
  super.data_offset = data_offset;
  super.name_table_size = data_offset - name_offset;
  super.image_bytes = image_bytes;
  super.node_count = node_count;
  super.root = root;
  return Result<Superblock>::Ok(super);
}

[[nodiscard]] Status LoadNode(const BlockDevice& device,
                              const Superblock& super,
                              uint32_t index) noexcept {
  uint8_t raw[kNodeSize];
  Status status = ReadBytes(
    device, super.node_offset + (static_cast<uint64_t>(index) * kNodeSize),
    std::span<uint8_t>(raw, sizeof(raw)));
  if (IsErr(status)) {
    return status;
  }
  if (Crc32(raw, kNodeCrcSize) != endian::ReadLe32(raw + 48U)) {
    return Status::kIoError;
  }

  const uint32_t type = endian::ReadLe32(raw);
  const uint32_t parent = endian::ReadLe32(raw + 16U);
  const uint32_t name_entry = endian::ReadLe32(raw + 20U);
  const uint64_t size = endian::ReadLe64(raw + 24U);
  const uint64_t data_block = endian::ReadLe64(raw + 32U);
  const uint64_t blocks = endian::ReadLe64(raw + 40U);
  const uint32_t data_checksum = endian::ReadLe32(raw + 52U);

  if (type != kNodeTypeFile && type != kNodeTypeDirectory) {
    return Status::kInvalidArg;
  }
  if (parent >= super.node_count ||
      (index == super.root && parent != super.root)) {
    return Status::kInvalidArg;
  }

  GslfsNode& node = g_nodes[index];
  node.index = index;
  node.parent = parent;
  node.mode = endian::ReadLe32(raw + 4U);
  node.data_checksum = data_checksum;
  node.node.type = type == kNodeTypeFile ? NodeType::kFile
                                         : NodeType::kDirectory;
  node.node.size = size;

  if (index == super.root) {
    node.node.name = "/";
    if (type != kNodeTypeDirectory) {
      return Status::kInvalidArg;
    }
  } else {
    status = ReadName(device, super.name_offset, super.name_table_size,
                      name_entry, node.name);
    if (IsErr(status) || node.name[0] == '\0') {
      return IsErr(status) ? status : Status::kInvalidArg;
    }
    node.node.name = node.name;
  }

  if (type == kNodeTypeFile) {
    node.node.ops = &kFileOps;
    if (size == 0U) {
      if (blocks != 0U) {
        return Status::kInvalidArg;
      }
      node.data_offset = 0;
    } else {
      if (data_block < (super.data_offset / kBlockSize) ||
          blocks != ((size + kBlockSize - 1U) / kBlockSize) ||
          blocks > (super.block_count - data_block)) {
        return Status::kInvalidArg;
      }
      node.data_offset = data_block * kBlockSize;
    }
  } else {
    node.node.ops = &kDirectoryOps;
    if (size != 0U || blocks != 0U) {
      return Status::kInvalidArg;
    }
    node.data_offset = 0;
  }
  node.node.fs_data = &node;
  return Status::kOk;
}

[[nodiscard]] Status LoadNodes(const BlockDevice& device,
                               const Superblock& super) noexcept {
  mem::Zero(g_nodes, sizeof(g_nodes));
  g_count = super.node_count;
  g_device = device;

  for (uint32_t index = 0; index < super.node_count; ++index) {
    const Status status = LoadNode(device, super, index);
    if (IsErr(status)) {
      return status;
    }
  }
  return Status::kOk;
}

[[nodiscard]] Status ValidateTree(const Superblock& super) noexcept {
  for (uint32_t index = 0; index < super.node_count; ++index) {
    if (index == super.root) {
      continue;
    }
    const uint32_t parent = g_nodes[index].parent;
    if (g_nodes[parent].node.type != NodeType::kDirectory) {
      return Status::kInvalidArg;
    }
    uint32_t cursor = index;
    bool reached_root = false;
    for (uint32_t depth = 0; depth < super.node_count; ++depth) {
      if (cursor == super.root) {
        reached_root = true;
        break;
      }
      cursor = g_nodes[cursor].parent;
    }
    if (!reached_root) {
      return Status::kInvalidArg;
    }
  }
  return Status::kOk;
}

[[nodiscard]] Status VerifyNodeData(const Superblock& super) noexcept {
  for (uint32_t index = 0; index < super.node_count; ++index) {
    if (g_nodes[index].node.type != NodeType::kFile) {
      continue;
    }
    if (!VerifyData(g_nodes[index], g_nodes[index].data_checksum)) {
      return Status::kIoError;
    }
  }
  return Status::kOk;
}

}  // namespace

Result<Node*> Mount(const BlockDevice& device) noexcept {
  if (g_ready) {
    return Result<Node*>::Err(Status::kExists);
  }
  if (device.read == nullptr || device.sector_count == 0U) {
    return Result<Node*>::Err(Status::kInvalidArg);
  }

  const Result<Superblock> super = ReadSuperblock(device);
  if (super.is_err()) {
    return Result<Node*>::Err(super.status());
  }

  Status status = LoadNodes(device, super.value());
  if (IsErr(status)) {
    return Result<Node*>::Err(status);
  }
  status = ValidateTree(super.value());
  if (IsErr(status)) {
    return Result<Node*>::Err(status);
  }
  status = VerifyNodeData(super.value());
  if (IsErr(status)) {
    return Result<Node*>::Err(status);
  }

  g_ready = true;
  LogDec(LogLevel::kInfo, "fs: gslfs nodes",
         static_cast<uint64_t>(g_count));
  return Result<Node*>::Ok(&g_nodes[super.value().root].node);
}

uint32_t NodeCount() noexcept { return g_count; }

}  // namespace og::fs::gslfs
