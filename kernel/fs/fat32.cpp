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
constexpr uint8_t kAttributeReadOnly = 0x01;
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

struct BootInfo {
  uint32_t sectors_per_cluster;
  uint32_t cluster_bytes;
  uint32_t reserved_sectors;
  uint32_t fat_sectors;
  uint32_t num_fats;
  uint32_t root_cluster;
  uint32_t data_start;
  uint32_t total_clusters;
};

struct FatNode {
  Node node;
  char name[kMaxName];
  uint32_t first_cluster;
  uint32_t dir_cluster;
};

struct ParsedEntry {
  DirEntry entry;
  uint32_t first_cluster;
};

struct EntryLocation {
  uint32_t cluster;
  uint32_t offset;
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
Status FileWrite(Node* node, uint64_t offset, std::span<const uint8_t> buffer,
                 uint64_t& out_written) noexcept;
Status FileTruncate(Node* node, uint64_t size) noexcept;
Status DirCreate(Node* directory, const char* name, NodeType type,
                 Node** out) noexcept;
Status DirRemove(Node* directory, Node* node) noexcept;
Status DirRename(Node* directory, Node* node, Node* new_directory,
                 const char* new_name) noexcept;
Status FatChmod(Node* node, uint32_t mode) noexcept;
[[nodiscard]] Result<bool> ParseEntry(const uint8_t* raw, char* long_name,
                                      bool& long_name_active,
                                      ParsedEntry& out) noexcept;
[[nodiscard]] Result<EntryLocation> FindEntry(FatNode* directory,
                                              const char* name) noexcept;

const NodeOps kFileOps{nullptr, &FileRead, &FileWrite, nullptr, nullptr,
                       nullptr, nullptr, &FileTruncate, nullptr, &FatChmod};
const NodeOps kDirectoryOps{&Lookup, nullptr, nullptr, &IterateDirectory,
                            &DirCreate, &DirRemove, &DirRename, nullptr,
                            nullptr, &FatChmod};

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

[[nodiscard]] uint64_t FatTimestampToUnix(uint16_t date,
                                            uint16_t time) noexcept {
  if (date == 0U) {
    return 0;
  }
  const uint32_t year = 1980U + ((date >> 9U) & 0x7FU);
  const uint32_t month = (date >> 5U) & 0x0FU;
  const uint32_t day = date & 0x1FU;
  const uint32_t hour = (time >> 11U) & 0x1FU;
  const uint32_t minute = (time >> 5U) & 0x3FU;
  const uint32_t second = (time & 0x1FU) * 2U;
  if (month < 1U || month > 12U || day < 1U || day > 31U ||
      hour > 23U || minute > 59U || second > 59U) {
    return 0;
  }
  int64_t adjusted_year = static_cast<int64_t>(year);
  const int64_t adjusted_month = static_cast<int64_t>(month);
  adjusted_year -= adjusted_month <= 2 ? 1 : 0;
  const int64_t era =
    (adjusted_year >= 0 ? adjusted_year : adjusted_year - 399) / 400;
  const uint32_t year_of_era =
    static_cast<uint32_t>(adjusted_year - era * 400);
  const int64_t shifted_month =
    adjusted_month + (adjusted_month > 2 ? -3 : 9);
  const uint32_t day_of_year =
    static_cast<uint32_t>((153 * shifted_month + 2) / 5 +
                           static_cast<int64_t>(day) - 1);
  const uint32_t day_of_era =
    year_of_era * 365U + year_of_era / 4U - year_of_era / 100U +
    day_of_year;
  const int64_t days =
    era * 146097 + static_cast<int64_t>(day_of_era) - 719468;
  if (days < 0) {
    return 0;
  }
  return static_cast<uint64_t>(days) * 86400U + hour * 3600U +
         minute * 60U + second;
}

void SetEntryTimestamp(uint8_t* entry, uint64_t seconds) noexcept {
  if (entry == nullptr || seconds == 0U) {
    return;
  }
  const uint64_t days = seconds / 86400U;
  const uint64_t clock = seconds % 86400U;
  const int64_t shifted = static_cast<int64_t>(days) + 719468;
  const int64_t era =
    (shifted >= 0 ? shifted : shifted - 146096) / 146097;
  const uint32_t day_of_era =
    static_cast<uint32_t>(shifted - era * 146097);
  const uint32_t year_of_era =
    (day_of_era - day_of_era / 1460U + day_of_era / 36524U -
     day_of_era / 146096U) /
    365U;
  int64_t year = static_cast<int64_t>(year_of_era) + era * 400;
  const uint32_t day_of_year =
    day_of_era - (365U * year_of_era + year_of_era / 4U -
                  year_of_era / 100U);
  const uint32_t month_part = (5U * day_of_year + 2U) / 153U;
  const uint32_t day =
    day_of_year - (153U * month_part + 2U) / 5U + 1U;
  const uint32_t month =
    month_part < 10U ? month_part + 3U : month_part - 9U;
  if (month <= 2U) {
    ++year;
  }
  if (year < 1980 || year > 2107) {
    return;
  }
  const uint32_t hour = static_cast<uint32_t>(clock / 3600U);
  const uint32_t minute = static_cast<uint32_t>((clock % 3600U) / 60U);
  const uint32_t second = static_cast<uint32_t>(clock % 60U);
  const uint16_t fat_date = static_cast<uint16_t>(
    ((year - 1980) << 9U) | (month << 5U) | day);
  const uint16_t fat_time = static_cast<uint16_t>(
    (hour << 11U) | (minute << 5U) | (second / 2U));
  entry[22] = static_cast<uint8_t>(fat_time & 0xFFU);
  entry[23] = static_cast<uint8_t>((fat_time >> 8U) & 0xFFU);
  entry[24] = static_cast<uint8_t>(fat_date & 0xFFU);
  entry[25] = static_cast<uint8_t>((fat_date >> 8U) & 0xFFU);
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

Status WriteSectors(uint64_t sector, const void* buffer, uint32_t count) noexcept {
  if (count == 0U || g_volume.device.write == nullptr) {
    return Status::kUnsupported;
  }
  const auto* bytes = static_cast<const uint8_t*>(buffer);
  const uint32_t chunk_sectors = kReadChunkBytes / kSectorSize;
  uint32_t done = 0;
  while (done < count) {
    uint32_t chunk = count - done;
    if (chunk > chunk_sectors) {
      chunk = chunk_sectors;
    }
    const std::span<const uint8_t> source(
      bytes + (static_cast<std::size_t>(done) * kSectorSize),
      static_cast<std::size_t>(chunk) * kSectorSize);
    const Status status = g_volume.device.write(
      g_volume.device.context, sector + done + g_volume.device.sector_offset,
      source);
    if (IsErr(status)) {
      return status;
    }
    done += chunk;
  }
  return Status::kOk;
}

[[nodiscard]] Status FlushVolume() noexcept {
  if (g_volume.device.flush == nullptr) {
    return Status::kUnsupported;
  }
  return g_volume.device.flush(g_volume.device.context);
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

[[nodiscard]] Result<uint32_t> AdvanceCluster(uint32_t cluster) noexcept {
  const Result<uint32_t> next = NextCluster(cluster);
  if (next.is_err()) {
    return Result<uint32_t>::Err(next.status());
  }
  if (next.value() == kClusterEnd) {
    return Result<uint32_t>::Err(Status::kIoError);
  }
  return Result<uint32_t>::Ok(next.value());
}

[[nodiscard]] Result<uint32_t> SeekCluster(uint32_t first_cluster,
                                           uint64_t cluster_index) noexcept {
  uint32_t cluster = first_cluster;
  for (uint64_t step = 0; step < cluster_index; ++step) {
    const Result<uint32_t> next = AdvanceCluster(cluster);
    if (next.is_err()) {
      return Result<uint32_t>::Err(next.status());
    }
    cluster = next.value();
  }
  return Result<uint32_t>::Ok(cluster);
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

[[nodiscard]] Status WriteCluster(uint32_t cluster, const uint8_t* buffer) noexcept {
  if (!ValidCluster(cluster)) {
    return Status::kInvalidArg;
  }
  const uint64_t sector = static_cast<uint64_t>(g_volume.data_start) +
                          (static_cast<uint64_t>(cluster - 2U) *
                           static_cast<uint64_t>(g_volume.sectors_per_cluster));
  return WriteSectors(sector, buffer, g_volume.sectors_per_cluster);
}

[[nodiscard]] Result<uint32_t> FatEntryValue(uint32_t cluster) noexcept {
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
  return Result<uint32_t>::Ok(value & 0x0FFFFFFFU);
}

[[nodiscard]] Status SetFatEntry(uint32_t cluster, uint32_t value) noexcept {
  if (!ValidCluster(cluster)) {
    return Status::kInvalidArg;
  }
  const uint64_t fat_offset = static_cast<uint64_t>(cluster) * 4U;
  const uint32_t offset = static_cast<uint32_t>(fat_offset % kSectorSize);
  const uint32_t base_sector =
    g_volume.reserved_sectors + static_cast<uint32_t>(fat_offset / kSectorSize);
  uint8_t sector[kSectorSize];
  for (uint32_t copy = 0; copy < g_volume.num_fats; ++copy) {
    const uint32_t sector_index = base_sector + (copy * g_volume.fat_sectors);
    Status status = ReadSectors(sector_index, sector, 1);
    if (IsErr(status)) {
      return status;
    }
    sector[offset] = static_cast<uint8_t>(value & 0xFFU);
    sector[offset + 1U] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
    sector[offset + 2U] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
    sector[offset + 3U] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
    status = WriteSectors(sector_index, sector, 1);
    if (IsErr(status)) {
      return status;
    }
  }
  return Status::kOk;
}

[[nodiscard]] Result<uint32_t> AllocateCluster() noexcept {
  for (uint32_t cluster = 2U; cluster < (g_volume.total_clusters + 2U);
       ++cluster) {
    const Result<uint32_t> value = FatEntryValue(cluster);
    if (value.is_err()) {
      return value;
    }
    if (value.value() != 0U) {
      continue;
    }
    Status status = SetFatEntry(cluster, kClusterEnd);
    if (IsErr(status)) {
      return Result<uint32_t>::Err(status);
    }
    mem::Zero(g_volume.cluster_buffer, g_volume.cluster_bytes);
    status = WriteCluster(cluster, g_volume.cluster_buffer);
    if (IsErr(status)) {
      return Result<uint32_t>::Err(status);
    }
    return Result<uint32_t>::Ok(cluster);
  }
  return Result<uint32_t>::Err(Status::kNoMem);
}

[[nodiscard]] Status EnsureClusterCount(FatNode* file,
                                        uint32_t required) noexcept {
  if (file == nullptr || required == 0U) {
    return Status::kInvalidArg;
  }
  uint32_t count = 0;
  uint32_t last = 0;
  uint32_t cluster = file->first_cluster;
  while (ValidCluster(cluster) && count < required) {
    last = cluster;
    ++count;
    const Result<uint32_t> next = NextCluster(cluster);
    if (next.is_err()) {
      return next.status();
    }
    if (next.value() == kClusterEnd) {
      break;
    }
    cluster = next.value();
  }

  if (file->first_cluster == 0U) {
    const Result<uint32_t> first = AllocateCluster();
    if (first.is_err()) {
      return first.status();
    }
    file->first_cluster = first.value();
    last = first.value();
    count = 1U;
  }

  while (count < required) {
    const Result<uint32_t> next = AllocateCluster();
    if (next.is_err()) {
      return next.status();
    }
    const Status status = SetFatEntry(last, next.value());
    if (IsErr(status)) {
      return status;
    }
    last = next.value();
    ++count;
  }
  return Status::kOk;
}

[[nodiscard]] Status UpdateDirectoryEntry(FatNode* file) noexcept {
  if (file == nullptr || !ValidCluster(file->dir_cluster)) {
    return Status::kNotFound;
  }
  uint32_t cluster = file->dir_cluster;
  uint32_t guard = 0;
  char long_name[kMaxName];
  mem::Zero(long_name, sizeof(long_name));
  bool long_name_active = false;

  while (ValidCluster(cluster)) {
    Status status = ReadCluster(cluster, g_volume.cluster_buffer);
    if (IsErr(status)) {
      return status;
    }
    for (uint32_t offset = 0; (offset + 32U) <= g_volume.cluster_bytes;
         offset += 32U) {
      uint8_t* entry = g_volume.cluster_buffer + offset;
      if (entry[0] == 0x00U) {
        return Status::kNotFound;
      }
      ParsedEntry parsed{};
      const Result<bool> emit =
        ParseEntry(entry, long_name, long_name_active, parsed);
      if (emit.is_err()) {
        return emit.status();
      }
      if (!emit.value() || !NameEquals(parsed.entry.name, file->name)) {
        continue;
      }
      const uint32_t first = file->first_cluster;
      entry[20] = static_cast<uint8_t>((first >> 16U) & 0xFFU);
      entry[21] = static_cast<uint8_t>((first >> 24U) & 0xFFU);
      entry[26] = static_cast<uint8_t>(first & 0xFFU);
      entry[27] = static_cast<uint8_t>((first >> 8U) & 0xFFU);
      const uint32_t size = static_cast<uint32_t>(file->node.size);
      entry[28] = static_cast<uint8_t>(size & 0xFFU);
      entry[29] = static_cast<uint8_t>((size >> 8U) & 0xFFU);
      entry[30] = static_cast<uint8_t>((size >> 16U) & 0xFFU);
      entry[31] = static_cast<uint8_t>((size >> 24U) & 0xFFU);
      SetEntryTimestamp(entry, file->node.mtime);
      return WriteCluster(cluster, g_volume.cluster_buffer);
    }
    const Result<uint32_t> next = NextCluster(cluster);
    if (next.is_err()) {
      return next.status();
    }
    if (next.value() == kClusterEnd) {
      return Status::kNotFound;
    }
    cluster = next.value();
    ++guard;
    if (guard > g_volume.total_clusters) {
      return Status::kIoError;
    }
  }
  return Status::kNotFound;
}

[[nodiscard]] Status BuildShortEntry(const char* name, uint8_t* entry) noexcept {
  if (name == nullptr || entry == nullptr) {
    return Status::kInvalidArg;
  }
  mem::Zero(entry, 32);
  for (uint32_t index = 0; index < 11U; ++index) {
    entry[index] = ' ';
  }
  uint32_t base_length = 0;
  uint32_t extension_length = 0;
  bool in_extension = false;
  for (std::size_t index = 0; name[index] != '\0'; ++index) {
    char value = name[index];
    if (value == '.') {
      if (in_extension) {
        return Status::kInvalidArg;
      }
      in_extension = true;
      continue;
    }
    if (value >= 'a' && value <= 'z') {
      value = static_cast<char>(value - 'a' + 'A');
    }
    if (!((value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9') ||
          value == '_' || value == '-')) {
      return Status::kInvalidArg;
    }
    if (in_extension) {
      if (extension_length >= 3U) {
        return Status::kInvalidArg;
      }
      entry[8U + extension_length] = static_cast<uint8_t>(value);
      ++extension_length;
    } else {
      if (base_length >= 8U) {
        return Status::kInvalidArg;
      }
      entry[base_length] = static_cast<uint8_t>(value);
      ++base_length;
    }
  }
  if (base_length == 0U) {
    return Status::kInvalidArg;
  }
  entry[11] = 0x20U;
  return Status::kOk;
}

[[nodiscard]] uint8_t ShortNameChecksum(const uint8_t* entry) noexcept {
  uint8_t sum = 0;
  for (uint32_t index = 0; index < 11U; ++index) {
    sum = static_cast<uint8_t>(((sum & 1U) << 7U) + (sum >> 1U) + entry[index]);
  }
  return sum;
}

[[nodiscard]] Status BuildShortAlias(const char* name, uint32_t index,
                                     uint8_t* entry) noexcept {
  if (name == nullptr || entry == nullptr || index == 0U || index > 9U) {
    return Status::kInvalidArg;
  }
  mem::Zero(entry, 32);
  for (uint32_t position = 0; position < 11U; ++position) {
    entry[position] = ' ';
  }
  uint32_t base_length = 0;
  uint32_t extension_length = 0;
  bool in_extension = false;
  for (std::size_t position = 0; name[position] != '\0'; ++position) {
    char value = name[position];
    if (value == '.') {
      in_extension = true;
      continue;
    }
    if (value >= 'a' && value <= 'z') {
      value = static_cast<char>(value - 'a' + 'A');
    }
    if (!((value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9') ||
          value == '_' || value == '-')) {
      continue;
    }
    if (in_extension) {
      if (extension_length < 3U) {
        entry[8U + extension_length] = static_cast<uint8_t>(value);
        ++extension_length;
      }
    } else if (base_length < 6U) {
      entry[base_length] = static_cast<uint8_t>(value);
      ++base_length;
    }
  }
  if (base_length == 0U) {
    return Status::kInvalidArg;
  }
  entry[base_length] = '~';
  entry[base_length + 1U] = static_cast<uint8_t>('0' + index);
  entry[11] = 0x20U;
  return Status::kOk;
}

[[nodiscard]] bool ShortNameExists(FatNode* directory,
                                   const uint8_t* entry) noexcept {
  if (directory == nullptr || entry == nullptr) {
    return false;
  }
  uint32_t cluster = directory->first_cluster;
  uint32_t guard = 0;
  while (ValidCluster(cluster)) {
    const Status status = ReadCluster(cluster, g_volume.cluster_buffer);
    if (IsErr(status)) {
      return true;
    }
    for (uint32_t offset = 0; (offset + 32U) <= g_volume.cluster_bytes;
         offset += 32U) {
      const uint8_t* slot = g_volume.cluster_buffer + offset;
      if (slot[0] == 0x00U) {
        return false;
      }
      if (slot[0] == 0xE5U || slot[11] == kAttributeLongName) {
        continue;
      }
      bool same = true;
      for (uint32_t index = 0; index < 11U; ++index) {
        if (slot[index] != entry[index]) {
          same = false;
          break;
        }
      }
      if (same) {
        return true;
      }
    }
    const Result<uint32_t> next = NextCluster(cluster);
    if (next.is_err() || next.value() == kClusterEnd) {
      return false;
    }
    cluster = next.value();
    ++guard;
    if (guard > g_volume.total_clusters) {
      return true;
    }
  }
  return false;
}

void BuildLongEntries(const char* name, uint32_t length,
                      const uint8_t* short_entry,
                      uint8_t entries[][32],
                      uint32_t count) noexcept {
  constexpr uint32_t kOffsets[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20,
                                      22, 24, 28, 30};
  const uint8_t checksum = ShortNameChecksum(short_entry);
  for (uint32_t sequence = count; sequence > 0U; --sequence) {
    uint8_t* entry = entries[count - sequence];
    mem::Zero(entry, 32);
    entry[0] = static_cast<uint8_t>(sequence);
    if (sequence == count) {
      entry[0] = static_cast<uint8_t>(entry[0] | 0x40U);
    }
    entry[11] = kAttributeLongName;
    entry[13] = checksum;
    const uint32_t base = (sequence - 1U) * 13U;
    for (uint32_t index = 0; index < 13U; ++index) {
      const uint32_t position = base + index;
      uint16_t code = 0xFFFFU;
      if (position < length) {
        code = static_cast<uint16_t>(
          static_cast<uint8_t>(name[position]));
      } else if (position == length) {
        code = 0U;
      }
      entry[kOffsets[index]] = static_cast<uint8_t>(code & 0xFFU);
      entry[kOffsets[index] + 1U] = static_cast<uint8_t>(code >> 8U);
    }
  }
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

void SetEntryCluster(uint8_t* entry, uint32_t cluster) noexcept {
  entry[20] = static_cast<uint8_t>((cluster >> 16U) & 0xFFU);
  entry[21] = static_cast<uint8_t>((cluster >> 24U) & 0xFFU);
  entry[26] = static_cast<uint8_t>(cluster & 0xFFU);
  entry[27] = static_cast<uint8_t>((cluster >> 8U) & 0xFFU);
}

void BuildDotEntry(uint8_t* entry, const char* name,
                   uint32_t cluster) noexcept {
  mem::Zero(entry, 32);
  for (uint32_t index = 0; index < 11U; ++index) {
    entry[index] = ' ';
  }
  for (uint32_t index = 0; name[index] != '\0'; ++index) {
    entry[index] = static_cast<uint8_t>(name[index]);
  }
  entry[11] = 0x10U;
  SetEntryCluster(entry, cluster);
}

Status DirCreate(Node* directory, const char* name, NodeType type,
                 Node** out) noexcept {
  if (out == nullptr) {
    return Status::kInvalidArg;
  }
  *out = nullptr;
  if (directory == nullptr || directory->type != NodeType::kDirectory ||
      name == nullptr || name[0] == '\0' ||
      (type != NodeType::kFile && type != NodeType::kDirectory)) {
    return Status::kUnsupported;
  }
  if (g_volume.device.write == nullptr) {
    return Status::kUnsupported;
  }

  auto* parent = reinterpret_cast<FatNode*>(directory);
  uint8_t entry[32];
  uint8_t long_entries[3][32]{};
  uint32_t long_count = 0;
  Status status = BuildShortEntry(name, entry);
  if (IsErr(status)) {
    const std::size_t name_length = mem::Length(name);
    if (name_length >= kMaxName) {
      return Status::kInvalidArg;
    }
    long_count = static_cast<uint32_t>((name_length + 12U) / 13U);
    if (long_count == 0U || long_count > 3U) {
      return Status::kInvalidArg;
    }
    bool alias_found = false;
    for (uint32_t index = 1U; index <= 9U; ++index) {
      if (IsErr(BuildShortAlias(name, index, entry))) {
        continue;
      }
      if (!ShortNameExists(parent, entry)) {
        alias_found = true;
        break;
      }
    }
    if (!alias_found) {
      return Status::kNoMem;
    }
    BuildLongEntries(name, static_cast<uint32_t>(name_length), entry,
                     long_entries, long_count);
  }

  uint32_t child_cluster = 0;
  if (type == NodeType::kDirectory) {
    const Result<uint32_t> allocated = AllocateCluster();
    if (allocated.is_err()) {
      return allocated.status();
    }
    child_cluster = allocated.value();
    mem::Zero(g_volume.cluster_buffer, g_volume.cluster_bytes);
    BuildDotEntry(g_volume.cluster_buffer, ".", child_cluster);
    BuildDotEntry(g_volume.cluster_buffer + 32U, "..",
                  parent->first_cluster);
    status = WriteCluster(child_cluster, g_volume.cluster_buffer);
    if (IsErr(status)) {
      return status;
    }
    entry[11] = 0x10U;
  } else {
    entry[11] = 0x20U;
  }
  SetEntryCluster(entry, child_cluster);
  const uint64_t now = NowSeconds();
  SetEntryTimestamp(entry, now);
  const uint32_t required = long_count + 1U;
  uint32_t cluster = parent->first_cluster;
  uint32_t guard = 0;
  while (ValidCluster(cluster)) {
    status = ReadCluster(cluster, g_volume.cluster_buffer);
    if (IsErr(status)) {
      return status;
    }
    for (uint32_t offset = 0;
         (offset + (required * 32U)) <= g_volume.cluster_bytes;
         offset += 32U) {
      bool free_run = true;
      for (uint32_t slot = 0; slot < required; ++slot) {
        const uint8_t* candidate =
          g_volume.cluster_buffer + offset + (slot * 32U);
        if (candidate[0] != 0x00U && candidate[0] != 0xE5U) {
          free_run = false;
          break;
        }
      }
      if (!free_run) {
        continue;
      }
      for (uint32_t slot = 0; slot < long_count; ++slot) {
        mem::Copy(g_volume.cluster_buffer + offset + (slot * 32U),
                  long_entries[slot], 32U);
      }
      mem::Copy(g_volume.cluster_buffer + offset + (long_count * 32U),
                entry, 32U);
      status = WriteCluster(cluster, g_volume.cluster_buffer);
      if (IsErr(status)) {
        return status;
      }
      status = FlushVolume();
      if (IsErr(status)) {
        return status;
      }

      FatNode* node = AllocateNode();
      if (node == nullptr) {
        return Status::kNoMem;
      }
      const std::size_t length = mem::Length(name);
      if (length >= kMaxName) {
        return Status::kInvalidArg;
      }
      mem::Copy(node->name, name, length + 1U);
      node->node.name = node->name;
      node->node.type = type;
      node->node.mode =
        type == NodeType::kDirectory ? kModeDirectory : kModeFile;
      node->node.mtime = now;
      node->node.size = 0;
      node->node.ops = type == NodeType::kDirectory ? &kDirectoryOps
                                                    : &kFileOps;
      node->first_cluster = child_cluster;
      node->dir_cluster = parent->first_cluster;
      parent->node.mtime = now;
      *out = &node->node;
      return Status::kOk;
    }
    const Result<uint32_t> next = NextCluster(cluster);
    if (next.is_err()) {
      return next.status();
    }
    if (next.value() == kClusterEnd) {
      break;
    }
    cluster = next.value();
    ++guard;
    if (guard > g_volume.total_clusters) {
      return Status::kIoError;
    }
  }
  return Status::kNoMem;
}

[[nodiscard]] bool DirectoryEmpty(FatNode* directory) noexcept {
  if (directory == nullptr || !ValidCluster(directory->first_cluster)) {
    return true;
  }
  uint32_t cluster = directory->first_cluster;
  uint32_t guard = 0;
  while (ValidCluster(cluster)) {
    const Status status = ReadCluster(cluster, g_volume.cluster_buffer);
    if (IsErr(status)) {
      return false;
    }
    for (uint32_t offset = 0; (offset + 32U) <= g_volume.cluster_bytes;
         offset += 32U) {
      const uint8_t* entry = g_volume.cluster_buffer + offset;
      if (entry[0] == 0x00U) {
        return true;
      }
      if (entry[0] == 0xE5U) {
        continue;
      }
      if (entry[0] == '.' &&
          (entry[1] == ' ' || (entry[1] == '.' && entry[2] == ' '))) {
        continue;
      }
      return false;
    }
    const Result<uint32_t> next = NextCluster(cluster);
    if (next.is_err() || next.value() == kClusterEnd) {
      return true;
    }
    cluster = next.value();
    ++guard;
    if (guard > g_volume.total_clusters) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] Status FreeChain(FatNode* node) noexcept {
  if (node == nullptr) {
    return Status::kInvalidArg;
  }
  uint32_t cluster = node->first_cluster;
  uint32_t guard = 0;
  while (ValidCluster(cluster)) {
    const Result<uint32_t> next = NextCluster(cluster);
    if (next.is_err()) {
      return next.status();
    }
    const Status status = SetFatEntry(cluster, 0);
    if (IsErr(status)) {
      return status;
    }
    if (next.value() == kClusterEnd) {
      break;
    }
    cluster = next.value();
    ++guard;
    if (guard > g_volume.total_clusters) {
      return Status::kIoError;
    }
  }
  node->first_cluster = 0;
  node->node.size = 0;
  return Status::kOk;
}

Status DirRemove(Node* directory, Node* node) noexcept {
  if (directory == nullptr || node == nullptr) {
    return Status::kInvalidArg;
  }
  auto* parent = reinterpret_cast<FatNode*>(directory);
  auto* target = reinterpret_cast<FatNode*>(node);
  const Result<EntryLocation> location = FindEntry(parent, target->name);
  if (location.is_err()) {
    return location.status();
  }
  if (target->node.type == NodeType::kDirectory &&
      !DirectoryEmpty(target)) {
    return Status::kBusy;
  }
  Status status = FreeChain(target);
  if (IsErr(status)) {
    return status;
  }
  status = ReadCluster(location.value().cluster, g_volume.cluster_buffer);
  if (IsErr(status)) {
    return status;
  }
  uint32_t clear_offset = location.value().offset;
  while (clear_offset >= 32U) {
    uint8_t* previous =
      g_volume.cluster_buffer + clear_offset - 32U;
    if (previous[11] != kAttributeLongName) {
      break;
    }
    previous[0] = 0xE5U;
    clear_offset -= 32U;
  }
  g_volume.cluster_buffer[location.value().offset] = 0xE5U;
  status = WriteCluster(location.value().cluster, g_volume.cluster_buffer);
  if (IsErr(status)) {
    return status;
  }
  parent->node.mtime = NowSeconds();
  return FlushVolume();
}

Status DirRename(Node* directory, Node* node, Node* new_directory,
                 const char* new_name) noexcept {
  if (directory == nullptr || node == nullptr || new_directory == nullptr ||
      new_name == nullptr || new_name[0] == '\0') {
    return Status::kInvalidArg;
  }
  if (directory != new_directory) {
    return Status::kUnsupported;
  }
  auto* parent = reinterpret_cast<FatNode*>(directory);
  auto* target = reinterpret_cast<FatNode*>(node);
  const Result<EntryLocation> location = FindEntry(parent, target->name);
  if (location.is_err()) {
    return location.status();
  }
  const Result<EntryLocation> existing = FindEntry(parent, new_name);
  if (existing.is_ok()) {
    return Status::kExists;
  }
  if (existing.status() != Status::kNotFound) {
    return existing.status();
  }

  uint8_t entry[32];
  Status status = BuildShortEntry(new_name, entry);
  if (IsErr(status)) {
    return Status::kUnsupported;
  }
  status = ReadCluster(location.value().cluster, g_volume.cluster_buffer);
  if (IsErr(status)) {
    return status;
  }
  uint32_t clear_offset = location.value().offset;
  while (clear_offset >= 32U) {
    uint8_t* previous =
      g_volume.cluster_buffer + clear_offset - 32U;
    if (previous[11] != kAttributeLongName) {
      break;
    }
    previous[0] = 0xE5U;
    clear_offset -= 32U;
  }
  mem::Copy(g_volume.cluster_buffer + location.value().offset, entry, 11U);
  status = WriteCluster(location.value().cluster, g_volume.cluster_buffer);
  if (IsErr(status)) {
    return status;
  }
  status = FlushVolume();
  if (IsErr(status)) {
    return status;
  }
  const std::size_t length = mem::Length(new_name);
  if (length >= kMaxName) {
    return Status::kInvalidArg;
  }
  mem::Copy(target->name, new_name, length + 1U);
  target->node.mtime = NowSeconds();
  parent->node.mtime = target->node.mtime;
  return Status::kOk;
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

// Parses one 32-byte directory slot. Returns false for slots that do not
// produce a node, such as deleted entries, long-name fragments, and volume
// labels. The end-of-directory marker is handled by the caller because it
// stops the whole walk.
[[nodiscard]] Result<bool> ParseEntry(const uint8_t* raw, char* long_name,
                                      bool& long_name_active,
                                      ParsedEntry& out) noexcept {
  if (raw[0] == 0xE5U) {
    long_name_active = false;
    return Result<bool>::Ok(false);
  }

  const uint8_t attribute = raw[11];
  if (attribute == kAttributeLongName) {
    AccumulateLongName(raw, long_name, long_name_active);
    return Result<bool>::Ok(false);
  }
  if ((attribute & kAttributeVolumeLabel) != 0U) {
    long_name_active = false;
    return Result<bool>::Ok(false);
  }

  out = ParsedEntry{};
  if (long_name_active && long_name[0] != '\0') {
    const std::size_t length = mem::Length(long_name);
    if (length >= sizeof(out.entry.name)) {
      return Result<bool>::Err(Status::kInvalidArg);
    }
    mem::Copy(out.entry.name, long_name, length + 1U);
  } else {
    BuildShortName(raw, out.entry.name);
  }
  long_name_active = false;

  const uint32_t cluster_high = static_cast<uint32_t>(raw[20]) |
                                (static_cast<uint32_t>(raw[21]) << 8U);
  const uint32_t cluster_low = static_cast<uint32_t>(raw[26]) |
                               (static_cast<uint32_t>(raw[27]) << 8U);
  out.first_cluster = (cluster_high << 16U) | cluster_low;

  out.entry.size = static_cast<uint32_t>(raw[28]) |
                   (static_cast<uint32_t>(raw[29]) << 8U) |
                   (static_cast<uint32_t>(raw[30]) << 16U) |
                   (static_cast<uint32_t>(raw[31]) << 24U);
  out.entry.type = ((attribute & kAttributeDirectory) != 0U)
                     ? NodeType::kDirectory
                     : NodeType::kFile;
  const uint16_t write_time = static_cast<uint16_t>(
    static_cast<uint16_t>(raw[22]) |
    (static_cast<uint16_t>(raw[23]) << 8U));
  const uint16_t write_date = static_cast<uint16_t>(
    static_cast<uint16_t>(raw[24]) |
    (static_cast<uint16_t>(raw[25]) << 8U));
  out.entry.mtime = FatTimestampToUnix(write_date, write_time);
  return Result<bool>::Ok(true);
}

[[nodiscard]] Result<EntryLocation> FindEntry(FatNode* directory,
                                              const char* name) noexcept {
  if (directory == nullptr || name == nullptr) {
    return Result<EntryLocation>::Err(Status::kInvalidArg);
  }
  uint32_t cluster = directory->first_cluster;
  uint32_t guard = 0;
  char long_name[kMaxName];
  mem::Zero(long_name, sizeof(long_name));
  bool long_name_active = false;

  while (ValidCluster(cluster)) {
    const Status read_status = ReadCluster(cluster, g_volume.cluster_buffer);
    if (IsErr(read_status)) {
      return Result<EntryLocation>::Err(read_status);
    }
    for (uint32_t offset = 0; (offset + 32U) <= g_volume.cluster_bytes;
         offset += 32U) {
      const uint8_t* entry = g_volume.cluster_buffer + offset;
      if (entry[0] == 0x00U) {
        return Result<EntryLocation>::Err(Status::kNotFound);
      }
      ParsedEntry parsed{};
      const Result<bool> emit =
        ParseEntry(entry, long_name, long_name_active, parsed);
      if (emit.is_err()) {
        return Result<EntryLocation>::Err(emit.status());
      }
      if (emit.value() && NameEquals(parsed.entry.name, name)) {
        return Result<EntryLocation>::Ok(
          EntryLocation{cluster, offset});
      }
    }
    const Result<uint32_t> next = NextCluster(cluster);
    if (next.is_err()) {
      return Result<EntryLocation>::Err(next.status());
    }
    if (next.value() == kClusterEnd) {
      break;
    }
    cluster = next.value();
    ++guard;
    if (guard > g_volume.total_clusters) {
      return Result<EntryLocation>::Err(Status::kIoError);
    }
  }
  return Result<EntryLocation>::Err(Status::kNotFound);
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

      ParsedEntry parsed{};
      const Result<bool> emit =
        ParseEntry(entry, long_name, long_name_active, parsed);
      if (emit.is_err()) {
        return emit.status();
      }
      if (!emit.value()) {
        continue;
      }
      if (visitor.visit(visitor.context, parsed.entry, parsed.first_cluster)) {
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
  node->node.mode = context.entry.type == NodeType::kDirectory
                      ? kModeDirectory
                      : kModeFile;
  node->node.mtime = context.entry.mtime;
  node->node.size = context.entry.size;
  node->node.ops = (context.entry.type == NodeType::kDirectory) ? &kDirectoryOps : &kFileOps;
  node->first_cluster = context.first_cluster;
  const auto* parent = reinterpret_cast<const FatNode*>(directory);
  node->dir_cluster = parent->first_cluster;
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
  const uint64_t cluster_index = offset / g_volume.cluster_bytes;
  if (cluster_index > g_volume.total_clusters) {
    return Status::kInvalidArg;
  }
  const Result<uint32_t> start =
    SeekCluster(fat_node->first_cluster, cluster_index);
  if (start.is_err()) {
    return start.status();
  }
  uint32_t cluster = start.value();

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
      const Result<uint32_t> next = AdvanceCluster(cluster);
      if (next.is_err()) {
        return next.status();
      }
      cluster = next.value();
    }
  }

  return Status::kOk;
}

Status FileWrite(Node* node, uint64_t offset,
                 std::span<const uint8_t> buffer,
                 uint64_t& out_written) noexcept {
  out_written = 0;
  if (node == nullptr) {
    return Status::kInvalidArg;
  }
  if (buffer.empty()) {
    return Status::kInvalidArg;
  }
  const uint64_t end = offset + buffer.size();
  if (end < offset) {
    return Status::kInvalidArg;
  }
  if (g_volume.device.write == nullptr) {
    return Status::kUnsupported;
  }

  auto* fat_node = reinterpret_cast<FatNode*>(node);
  const uint64_t required =
    (end + g_volume.cluster_bytes - 1U) / g_volume.cluster_bytes;
  if (required > g_volume.total_clusters) {
    return Status::kNoMem;
  }
  Status ensure = EnsureClusterCount(fat_node, static_cast<uint32_t>(required));
  if (IsErr(ensure)) {
    return ensure;
  }

  const uint64_t cluster_index = offset / g_volume.cluster_bytes;
  if (cluster_index > g_volume.total_clusters) {
    return Status::kInvalidArg;
  }
  const Result<uint32_t> start =
    SeekCluster(fat_node->first_cluster, cluster_index);
  if (start.is_err()) {
    return start.status();
  }
  uint32_t cluster = start.value();

  uint64_t within = offset % g_volume.cluster_bytes;
  uint64_t remaining = buffer.size();
  const uint8_t* in = buffer.data();

  while (remaining > 0U) {
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
    mem::Copy(g_volume.cluster_buffer + within, in, chunk);
    const Status write_status = WriteCluster(cluster, g_volume.cluster_buffer);
    if (IsErr(write_status)) {
      return write_status;
    }
    out_written += chunk;
    in += chunk;
    remaining -= chunk;
    within = 0;

    if (remaining > 0U) {
      const Result<uint32_t> next = AdvanceCluster(cluster);
      if (next.is_err()) {
        return next.status();
      }
      cluster = next.value();
    }
  }

  if (end > node->size) {
    node->size = end;
  }
  node->mtime = NowSeconds();
  const Status update = UpdateDirectoryEntry(fat_node);
  if (IsErr(update)) {
    return update;
  }
  return FlushVolume();
}

Status FileTruncate(Node* node, uint64_t size) noexcept {
  if (node == nullptr) {
    return Status::kInvalidArg;
  }
  if (size != 0U) {
    return Status::kUnsupported;
  }
  auto* fat_node = reinterpret_cast<FatNode*>(node);
  Status status = FreeChain(fat_node);
  if (IsErr(status)) {
    return status;
  }
  node->mtime = NowSeconds();
  status = UpdateDirectoryEntry(fat_node);
  if (IsErr(status)) {
    return status;
  }
  return FlushVolume();
}

Status FatChmod(Node* node, uint32_t mode) noexcept {
  if (node == nullptr || mode > 0777U) {
    return Status::kInvalidArg;
  }
  auto* file = reinterpret_cast<FatNode*>(node);
  if (node->type == NodeType::kFile) {
    if (g_volume.device.write == nullptr) {
      return Status::kUnsupported;
    }
    FatNode parent{};
    parent.first_cluster = file->dir_cluster;
    const Result<EntryLocation> location = FindEntry(&parent, file->name);
    if (location.is_err()) {
      return location.status();
    }
    Status status =
      ReadCluster(location.value().cluster, g_volume.cluster_buffer);
    if (IsErr(status)) {
      return status;
    }
    uint8_t& attribute =
      g_volume.cluster_buffer[location.value().offset + 11U];
    if ((mode & 0200U) == 0U) {
      attribute |= kAttributeReadOnly;
    } else {
      attribute &= static_cast<uint8_t>(~kAttributeReadOnly);
    }
    status = WriteCluster(location.value().cluster, g_volume.cluster_buffer);
    if (IsErr(status)) {
      return status;
    }
    status = FlushVolume();
    if (IsErr(status)) {
      return status;
    }
  }
  node->mode = mode & 0777U;
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

// Reads and validates the boot sector. The caller sets g_volume.device
// first because ReadSectors is volume based.
[[nodiscard]] Result<BootInfo> ParseBootSector() noexcept {
  uint8_t boot_sector[kSectorSize];
  const Status status = ReadSectors(0, boot_sector, 1);
  if (IsErr(status)) {
    return Result<BootInfo>::Err(status);
  }
  if (boot_sector[510] != 0x55U || boot_sector[511] != 0xAAU) {
    return Result<BootInfo>::Err(Status::kNoDevice);
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
    return Result<BootInfo>::Err(Status::kUnsupported);
  }
  if (sectors_per_cluster == 0U ||
      (sectors_per_cluster & (sectors_per_cluster - 1U)) != 0U) {
    return Result<BootInfo>::Err(Status::kInvalidArg);
  }
  if (reserved_sectors == 0U || num_fats == 0U || fat_sectors == 0U) {
    return Result<BootInfo>::Err(Status::kInvalidArg);
  }
  if (root_cluster < 2U) {
    return Result<BootInfo>::Err(Status::kInvalidArg);
  }
  if (total_sectors == 0U) {
    return Result<BootInfo>::Err(Status::kInvalidArg);
  }

  const uint32_t cluster_bytes = sectors_per_cluster * kSectorSize;
  if (cluster_bytes > kMaxClusterBytes) {
    return Result<BootInfo>::Err(Status::kUnsupported);
  }
  const uint32_t data_start = reserved_sectors + (num_fats * fat_sectors);
  if (data_start >= total_sectors) {
    return Result<BootInfo>::Err(Status::kInvalidArg);
  }

  BootInfo info{};
  info.sectors_per_cluster = sectors_per_cluster;
  info.cluster_bytes = cluster_bytes;
  info.reserved_sectors = reserved_sectors;
  info.fat_sectors = fat_sectors;
  info.num_fats = num_fats;
  info.root_cluster = root_cluster;
  info.data_start = data_start;
  info.total_clusters = (total_sectors - data_start) / sectors_per_cluster;
  return Result<BootInfo>::Ok(info);
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

  g_volume.device = device;
  const Result<BootInfo> parsed = ParseBootSector();
  if (parsed.is_err()) {
    return parsed.status();
  }
  const BootInfo& boot = parsed.value();

  g_volume.sectors_per_cluster = boot.sectors_per_cluster;
  g_volume.cluster_bytes = boot.cluster_bytes;
  g_volume.reserved_sectors = boot.reserved_sectors;
  g_volume.fat_sectors = boot.fat_sectors;
  g_volume.num_fats = boot.num_fats;
  g_volume.root_cluster = boot.root_cluster;
  g_volume.data_start = boot.data_start;
  g_volume.total_clusters = boot.total_clusters;

  const Result<void*> buffer = mem::HeapAlloc(boot.cluster_bytes);
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
  root->node.mode = kModeDirectory;
  root->node.size = 0;
  root->node.ops = &kDirectoryOps;
  root->first_cluster = boot.root_cluster;

  *root_out = &root->node;
  return Status::kOk;
}

}  // namespace og::fs::fat32
