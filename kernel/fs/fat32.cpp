#include "og/fs/fat32.h"

#include "memory.h"
#include "og/mem/heap.h"

namespace og::fs::fat32 {

namespace {

constexpr uint32_t kSectorSize = 512;
constexpr uint32_t kMaxNodes = 96;
constexpr uint32_t kMaxName = 64;
constexpr uint32_t kClusterEnd = 0xFFFFFFFFU;
constexpr uint32_t kMaxClusterBytes = 32768;
constexpr uint32_t kReadChunkBytes = 4096;
constexpr uint8_t kAttributeLongName = 0x0F;
constexpr uint8_t kAttributeVolumeLabel = 0x08;
constexpr uint8_t kAttributeDirectory = 0x10;

struct Volume {
  BlockDevice device;
  uint32_t sectors_per_cluster;
  uint32_t cluster_bytes;
  uint32_t reserved_sectors;
  uint32_t fat_sectors;
  uint32_t num_fats;
  uint32_t root_cluster;
  uint32_t data_start;
  uint32_t total_clusters;
  uint8_t* cluster_buffer;
};

struct FatNode {
  Node node;
  char name[kMaxName];
  uint32_t first_cluster;
};

struct Visitor {
  bool (*visit)(void* context, const DirEntry& entry, uint32_t first_cluster) noexcept;
  void* context;
};

Volume g_volume{};
FatNode g_nodes[kMaxNodes];
uint32_t g_node_count = 0;

Status Lookup(Node* directory, const char* name, Node** out) noexcept;
Status IterateDirectory(Node* directory, uint32_t index, DirEntry& entry,
                        bool& found) noexcept;
Status FileRead(Node* node, uint64_t offset, std::span<uint8_t> buffer,
                uint64_t& out_read) noexcept;

const NodeOps kFileOps{nullptr, &FileRead, nullptr, nullptr};
const NodeOps kDirectoryOps{&Lookup, nullptr, nullptr, &IterateDirectory};

[[nodiscard]] char ToUpper(char value) noexcept {
  if (value >= 'a' && value <= 'z') {
    return static_cast<char>(value - 'a' + 'A');
  }
  return value;
}

[[nodiscard]] bool NameEquals(const char* lhs, const char* rhs) noexcept {
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }
  std::size_t index = 0;
  while (lhs[index] != '\0' && rhs[index] != '\0') {
    if (ToUpper(lhs[index]) != ToUpper(rhs[index])) {
      return false;
    }
    ++index;
  }
  return lhs[index] == '\0' && rhs[index] == '\0';
}

[[nodiscard]] bool ValidCluster(uint32_t cluster) noexcept {
  if (cluster < 2U) {
    return false;
  }
  return cluster < (g_volume.total_clusters + 2U);
}

[[nodiscard]] Status ReadSectors(uint64_t sector, void* buffer, uint32_t count) noexcept {
  if (count == 0) {
    return Status::kInvalidArg;
  }
  auto* bytes = static_cast<uint8_t*>(buffer);
  const uint32_t chunk_sectors = kReadChunkBytes / kSectorSize;
  uint32_t done = 0;
  while (done < count) {
    uint32_t chunk = count - done;
    if (chunk > chunk_sectors) {
      chunk = chunk_sectors;
    }
    const std::span<uint8_t> target(bytes + (static_cast<std::size_t>(done) * kSectorSize),
                                    static_cast<std::size_t>(chunk) * kSectorSize);
    const Status status = g_volume.device.read(
      g_volume.device.context, sector + done + g_volume.device.sector_offset, target);
    if (IsErr(status)) {
      return status;
    }
    done += chunk;
  }
  return Status::kOk;
}

[[nodiscard]] Result<uint32_t> NextCluster(uint32_t cluster) noexcept {
  if (!ValidCluster(cluster)) {
    return Result<uint32_t>::Err(Status::kInvalidArg);
  }
  uint8_t sector[kSectorSize];
  const uint64_t fat_offset = static_cast<uint64_t>(cluster) * 4U;
  const uint64_t fat_sector = static_cast<uint64_t>(g_volume.reserved_sectors) +
                              (fat_offset / kSectorSize);
  const uint32_t offset = static_cast<uint32_t>(fat_offset % kSectorSize);
  const Status status = ReadSectors(fat_sector, sector, 1);
  if (IsErr(status)) {
    return Result<uint32_t>::Err(status);
  }

  uint32_t value = static_cast<uint32_t>(sector[offset]);
  value |= static_cast<uint32_t>(sector[offset + 1U]) << 8U;
  value |= static_cast<uint32_t>(sector[offset + 2U]) << 16U;
  value |= static_cast<uint32_t>(sector[offset + 3U]) << 24U;
  value &= 0x0FFFFFFFU;

  if (value >= 0x0FFFFFF8U) {
    return Result<uint32_t>::Ok(kClusterEnd);
  }
  if (value == 0U || value == 0x0FFFFFF7U) {
    return Result<uint32_t>::Err(Status::kIoError);
  }
  return Result<uint32_t>::Ok(value);
}

[[nodiscard]] Status ReadCluster(uint32_t cluster, uint8_t* buffer) noexcept {
  if (!ValidCluster(cluster)) {
    return Status::kInvalidArg;
  }
  const uint64_t sector = static_cast<uint64_t>(g_volume.data_start) +
                          (static_cast<uint64_t>(cluster - 2U) *
                           static_cast<uint64_t>(g_volume.sectors_per_cluster));
  return ReadSectors(sector, buffer, g_volume.sectors_per_cluster);
}

[[nodiscard]] FatNode* AllocateNode() noexcept {
  if (g_node_count >= kMaxNodes) {
    return nullptr;
  }
  FatNode* node = &g_nodes[g_node_count];
  ++g_node_count;
  mem::Zero(node, sizeof(*node));
  return node;
}

void BuildShortName(const uint8_t* entry, char* out) noexcept {
  std::size_t length = 0;
  for (uint32_t index = 0; index < 8U; ++index) {
    const char value = static_cast<char>(entry[index]);
    if (value == ' ') {
      break;
    }
    if (length + 1U < kMaxName) {
      out[length] = value;
      ++length;
    }
  }
  if (entry[8] != ' ') {
    if (length + 1U < kMaxName) {
      out[length] = '.';
      ++length;
    }
    for (uint32_t index = 8U; index < 11U; ++index) {
      const char value = static_cast<char>(entry[index]);
      if (value == ' ') {
        break;
      }
      if (length + 1U < kMaxName) {
        out[length] = value;
        ++length;
      }
    }
  }
  out[length] = '\0';
}

void AccumulateLongName(const uint8_t* entry, char* text, bool& active) noexcept {
  constexpr uint32_t kOffsets[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};

  if ((entry[0] & 0x40U) != 0U) {
    mem::Zero(text, kMaxName);
    active = true;
  }
  if (!active) {
    return;
  }

  const uint32_t sequence = static_cast<uint32_t>(entry[0] & 0x1FU);
  if (sequence == 0U) {
    return;
  }
  const uint32_t base = (sequence - 1U) * 13U;

  for (uint32_t index = 0; index < 13U; ++index) {
    const uint32_t low = static_cast<uint32_t>(entry[kOffsets[index]]);
    const uint32_t high = static_cast<uint32_t>(entry[kOffsets[index] + 1U]) << 8U;
    const uint16_t code = static_cast<uint16_t>(low | high);
    if (code == 0x0000U || code == 0xFFFFU) {
      continue;
    }
    const uint32_t position = base + index;
    if (position >= (kMaxName - 1U)) {
      continue;
    }
    text[position] = static_cast<char>(code & 0xFFU);
  }
}

[[nodiscard]] Status WalkDirectory(Node* directory, const Visitor& visitor) noexcept {
  if (directory == nullptr || visitor.visit == nullptr) {
    return Status::kInvalidArg;
  }
  const auto* fat_directory = reinterpret_cast<const FatNode*>(directory);
  uint32_t cluster = fat_directory->first_cluster;
  uint32_t guard = 0;

  char long_name[kMaxName];
  mem::Zero(long_name, sizeof(long_name));
  bool long_name_active = false;

  while (ValidCluster(cluster)) {
    const Status read_status = ReadCluster(cluster, g_volume.cluster_buffer);
    if (IsErr(read_status)) {
      return read_status;
    }

    for (uint32_t offset = 0; (offset + 32U) <= g_volume.cluster_bytes; offset += 32U) {
      const uint8_t* entry = g_volume.cluster_buffer + offset;
      if (entry[0] == 0x00U) {
        return Status::kOk;
      }
      if (entry[0] == 0xE5U) {
        long_name_active = false;
        continue;
      }

      const uint8_t attribute = entry[11];
      if (attribute == kAttributeLongName) {
        AccumulateLongName(entry, long_name, long_name_active);
        continue;
      }
      if ((attribute & kAttributeVolumeLabel) != 0U) {
        long_name_active = false;
        continue;
      }

      DirEntry result{};
      if (long_name_active && long_name[0] != '\0') {
        const std::size_t length = mem::Length(long_name);
        if (length >= sizeof(result.name)) {
          return Status::kInvalidArg;
        }
        mem::Copy(result.name, long_name, length + 1U);
      } else {
        BuildShortName(entry, result.name);
      }
      long_name_active = false;

      const uint32_t cluster_high = static_cast<uint32_t>(entry[20]) |
                                    (static_cast<uint32_t>(entry[21]) << 8U);
      const uint32_t cluster_low = static_cast<uint32_t>(entry[26]) |
                                   (static_cast<uint32_t>(entry[27]) << 8U);
      const uint32_t first_cluster = (cluster_high << 16U) | cluster_low;

      const uint32_t size = static_cast<uint32_t>(entry[28]) |
                            (static_cast<uint32_t>(entry[29]) << 8U) |
                            (static_cast<uint32_t>(entry[30]) << 16U) |
                            (static_cast<uint32_t>(entry[31]) << 24U);

      result.type = ((attribute & kAttributeDirectory) != 0U) ? NodeType::kDirectory
                                                              : NodeType::kFile;
      result.size = size;

      if (visitor.visit(visitor.context, result, first_cluster)) {
        return Status::kOk;
      }
    }

    const Result<uint32_t> next = NextCluster(cluster);
    if (next.is_err()) {
      return next.status();
    }
    if (next.value() == kClusterEnd) {
      return Status::kOk;
    }
    cluster = next.value();
    ++guard;
    if (guard > g_volume.total_clusters) {
      return Status::kIoError;
    }
  }

  return Status::kOk;
}

struct IterateContext {
  uint32_t index;
  uint32_t seen;
  DirEntry entry;
  bool found;
};

bool IterateVisitor(void* raw, const DirEntry& entry, uint32_t /*cluster*/) noexcept {
  auto* context = static_cast<IterateContext*>(raw);
  if (context->seen == context->index) {
    context->entry = entry;
    context->found = true;
    return true;
  }
  ++context->seen;
  return false;
}

struct LookupContext {
  const char* name;
  DirEntry entry;
  uint32_t first_cluster;
  bool found;
};

bool LookupVisitor(void* raw, const DirEntry& entry, uint32_t cluster) noexcept {
  auto* context = static_cast<LookupContext*>(raw);
  if (!NameEquals(entry.name, context->name)) {
    return false;
  }
  context->entry = entry;
  context->first_cluster = cluster;
  context->found = true;
  return true;
}

Status Lookup(Node* directory, const char* name, Node** out) noexcept {
  if (out == nullptr) {
    return Status::kInvalidArg;
  }
  *out = nullptr;
  if (name == nullptr) {
    return Status::kInvalidArg;
  }

  LookupContext context{name, DirEntry{}, 0, false};
  const Visitor visitor{&LookupVisitor, &context};
  const Status status = WalkDirectory(directory, visitor);
  if (IsErr(status)) {
    return status;
  }
  if (!context.found) {
    return Status::kNotFound;
  }

  FatNode* node = AllocateNode();
  if (node == nullptr) {
    return Status::kNoMem;
  }
  const std::size_t length = mem::Length(context.entry.name);
  if (length >= kMaxName) {
    return Status::kInvalidArg;
  }
  mem::Copy(node->name, context.entry.name, length + 1U);
  node->node.name = node->name;
  node->node.type = context.entry.type;
  node->node.size = context.entry.size;
  node->node.ops = (context.entry.type == NodeType::kDirectory) ? &kDirectoryOps : &kFileOps;
  node->first_cluster = context.first_cluster;
  *out = &node->node;
  return Status::kOk;
}

Status FileRead(Node* node, uint64_t offset, std::span<uint8_t> buffer,
                uint64_t& out_read) noexcept {
  out_read = 0;
  if (node == nullptr) {
    return Status::kInvalidArg;
  }
  if (buffer.empty()) {
    return Status::kInvalidArg;
  }
  if (offset >= node->size) {
    return Status::kOk;
  }

  const auto* fat_node = reinterpret_cast<const FatNode*>(node);
  uint32_t cluster = fat_node->first_cluster;
  const uint64_t cluster_index = offset / g_volume.cluster_bytes;
  if (cluster_index > g_volume.total_clusters) {
    return Status::kInvalidArg;
  }
  for (uint64_t step = 0; step < cluster_index; ++step) {
    const Result<uint32_t> next = NextCluster(cluster);
    if (next.is_err()) {
      return next.status();
    }
    if (next.value() == kClusterEnd) {
      return Status::kIoError;
    }
    cluster = next.value();
  }

  uint64_t within = offset % g_volume.cluster_bytes;
  uint64_t remaining = node->size - offset;
  if (remaining > buffer.size()) {
    remaining = buffer.size();
  }

  uint8_t* out = buffer.data();
  while (remaining > 0) {
    if (!ValidCluster(cluster)) {
      return Status::kIoError;
    }
    const Status read_status = ReadCluster(cluster, g_volume.cluster_buffer);
    if (IsErr(read_status)) {
      return read_status;
    }

    uint64_t chunk = g_volume.cluster_bytes - within;
    if (chunk > remaining) {
      chunk = remaining;
    }
    mem::Copy(out, g_volume.cluster_buffer + within, chunk);
    out += chunk;
    out_read += chunk;
    remaining -= chunk;
    within = 0;

    if (remaining > 0) {
      const Result<uint32_t> next = NextCluster(cluster);
      if (next.is_err()) {
        return next.status();
      }
      if (next.value() == kClusterEnd) {
        return Status::kIoError;
      }
      cluster = next.value();
    }
  }

  return Status::kOk;
}

Status IterateDirectory(Node* directory, uint32_t index, DirEntry& entry,
                        bool& found) noexcept {
  found = false;
  IterateContext context{index, 0, DirEntry{}, false};
  const Visitor visitor{&IterateVisitor, &context};
  const Status status = WalkDirectory(directory, visitor);
  if (IsErr(status)) {
    return status;
  }
  if (context.found) {
    entry = context.entry;
    found = true;
  }
  return Status::kOk;
}

}  // namespace

Status Mount(const BlockDevice& device, Node** root_out) noexcept {
  if (root_out == nullptr) {
    return Status::kInvalidArg;
  }
  *root_out = nullptr;
  if (device.read == nullptr) {
    return Status::kInvalidArg;
  }
  if (device.sector_count == 0) {
    return Status::kInvalidArg;
  }

  uint8_t boot_sector[kSectorSize];
  g_volume.device = device;
  Status status = ReadSectors(0, boot_sector, 1);
  if (IsErr(status)) {
    return status;
  }
  if (boot_sector[510] != 0x55U || boot_sector[511] != 0xAAU) {
    return Status::kNoDevice;
  }

  const uint32_t bytes_per_sector = static_cast<uint32_t>(boot_sector[11]) |
                                    (static_cast<uint32_t>(boot_sector[12]) << 8U);
  const uint32_t sectors_per_cluster = static_cast<uint32_t>(boot_sector[13]);
  const uint32_t reserved_sectors = static_cast<uint32_t>(boot_sector[14]) |
                                    (static_cast<uint32_t>(boot_sector[15]) << 8U);
  const uint32_t num_fats = static_cast<uint32_t>(boot_sector[16]);
  const uint32_t fat_sectors = static_cast<uint32_t>(boot_sector[36]) |
                               (static_cast<uint32_t>(boot_sector[37]) << 8U) |
                               (static_cast<uint32_t>(boot_sector[38]) << 16U) |
                               (static_cast<uint32_t>(boot_sector[39]) << 24U);
  const uint32_t root_cluster = static_cast<uint32_t>(boot_sector[44]) |
                                (static_cast<uint32_t>(boot_sector[45]) << 8U) |
                                (static_cast<uint32_t>(boot_sector[46]) << 16U) |
                                (static_cast<uint32_t>(boot_sector[47]) << 24U);
  const uint32_t total_sectors = static_cast<uint32_t>(boot_sector[32]) |
                                 (static_cast<uint32_t>(boot_sector[33]) << 8U) |
                                 (static_cast<uint32_t>(boot_sector[34]) << 16U) |
                                 (static_cast<uint32_t>(boot_sector[35]) << 24U);

  if (bytes_per_sector != kSectorSize) {
    return Status::kUnsupported;
  }
  if (sectors_per_cluster == 0U || (sectors_per_cluster & (sectors_per_cluster - 1U)) != 0U) {
    return Status::kInvalidArg;
  }
  if (reserved_sectors == 0U || num_fats == 0U || fat_sectors == 0U) {
    return Status::kInvalidArg;
  }
  if (root_cluster < 2U) {
    return Status::kInvalidArg;
  }
  if (total_sectors == 0U) {
    return Status::kInvalidArg;
  }

  const uint32_t cluster_bytes = sectors_per_cluster * kSectorSize;
  if (cluster_bytes > kMaxClusterBytes) {
    return Status::kUnsupported;
  }
  const uint32_t data_start = reserved_sectors + (num_fats * fat_sectors);
  if (data_start >= total_sectors) {
    return Status::kInvalidArg;
  }

  g_volume.sectors_per_cluster = sectors_per_cluster;
  g_volume.cluster_bytes = cluster_bytes;
  g_volume.reserved_sectors = reserved_sectors;
  g_volume.fat_sectors = fat_sectors;
  g_volume.num_fats = num_fats;
  g_volume.root_cluster = root_cluster;
  g_volume.data_start = data_start;
  g_volume.total_clusters = (total_sectors - data_start) / sectors_per_cluster;

  const Result<void*> buffer = mem::HeapAlloc(cluster_bytes);
  if (buffer.is_err()) {
    return buffer.status();
  }
  g_volume.cluster_buffer = static_cast<uint8_t*>(buffer.value());

  g_node_count = 0;
  FatNode* root = AllocateNode();
  if (root == nullptr) {
    return Status::kNoMem;
  }
  root->node.name = "/";
  root->node.type = NodeType::kDirectory;
  root->node.size = 0;
  root->node.ops = &kDirectoryOps;
  root->first_cluster = root_cluster;

  *root_out = &root->node;
  return Status::kOk;
}

}  // namespace og::fs::fat32
