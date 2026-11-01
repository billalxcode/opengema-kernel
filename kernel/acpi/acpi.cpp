#include "og/acpi/acpi.h"

#include "memory.h"

namespace og::acpi {

namespace {

constexpr uint32_t kRsdpChecksumLength = 20;
constexpr uint32_t kRsdpV2Length = 36;
constexpr uint32_t kSdtHeaderLength = 36;
constexpr uint32_t kMaxTableLength = 1U << 20;
constexpr uint32_t kMaxRootEntries = 256;
constexpr uint32_t kMadtHeaderLength = 44;
constexpr uint32_t kMadtGiccMinimumLength = 68;
constexpr uint32_t kMadtGiccFlagsOffset = 12;
constexpr uint32_t kMadtGiccGicrOffset = 60;
constexpr uint32_t kMadtGicdMinimumLength = 24;
constexpr uint32_t kMadtGicdBaseOffset = 8;
// SPCR keeps its UART base in a generic address structure at offset 40.
constexpr uint32_t kSpcrBaseOffset = 44;
constexpr uint32_t kSpcrMinimumLength = 48;
// MCFG holds allocation entries after the 44 byte header.
constexpr uint32_t kMcfgFirstEntry = 44;
constexpr uint32_t kMcfgMinimumLength = 60;
constexpr uint32_t kMcfgStartBusOffset = 10;
constexpr uint32_t kMcfgEndBusOffset = 11;

constexpr uint8_t kMadtGenericInterrupt = 0x0B;
constexpr uint8_t kMadtGenericDistributor = 0x0C;
constexpr uint8_t kMadtEnabled = 1U;

struct SdtHeader {
  char signature[4];
  uint32_t length;
  uint8_t revision;
  uint8_t checksum;
  char oem_id[6];
  char oem_table_id[8];
  uint32_t oem_revision;
  uint32_t creator_id;
  uint32_t creator_revision;
};

static_assert(sizeof(SdtHeader) == kSdtHeaderLength);

Info g_info{};
Table g_tables[kMaxTables]{};

[[nodiscard]] uint8_t Read8(uint64_t address) noexcept {
  return *reinterpret_cast<const uint8_t*>(address);
}

[[nodiscard]] uint32_t Read32(uint64_t address) noexcept {
  uint32_t value = 0;
  mem::Copy(&value, reinterpret_cast<const void*>(address), sizeof(value));
  return value;
}

[[nodiscard]] uint64_t Read64(uint64_t address) noexcept {
  uint64_t value = 0;
  mem::Copy(&value, reinterpret_cast<const void*>(address), sizeof(value));
  return value;
}

[[nodiscard]] bool SignatureIs(uint64_t address, const char* signature,
                               uint32_t length) noexcept {
  return mem::Equal(reinterpret_cast<const void*>(address), signature, length);
}

[[nodiscard]] bool ChecksumOk(uint64_t address, uint32_t length) noexcept {
  const auto* bytes = reinterpret_cast<const uint8_t*>(address);
  uint8_t sum = 0;
  for (uint32_t index = 0; index < length; ++index) {
    sum = static_cast<uint8_t>(sum + bytes[index]);
  }
  return sum == 0U;
}

[[nodiscard]] bool LoadTable(uint64_t address, Table& table) noexcept {
  if (address == 0U) {
    return false;
  }
  const uint32_t length = Read32(address + 4U);
  if (length < kSdtHeaderLength || length > kMaxTableLength) {
    return false;
  }
  if (!ChecksumOk(address, length)) {
    return false;
  }
  table.address = address;
  table.length = length;
  table.revision = Read8(address + 8U);
  mem::Copy(table.signature, reinterpret_cast<const void*>(address),
            kSignatureLength);
  table.signature[kSignatureLength] = '\0';
  return true;
}

void ParseMadt() noexcept {
  const Result<Table> table = FindTable("APIC");
  if (table.is_err() || table.value().length < kMadtHeaderLength) {
    return;
  }
  const uint64_t address = table.value().address;
  const uint32_t length = table.value().length;
  uint32_t offset = kMadtHeaderLength;
  while ((offset + 2U) <= length) {
    const uint8_t type = Read8(address + offset);
    const uint8_t entry_length = Read8(address + offset + 1U);
    if (entry_length < 2U || (offset + entry_length) > length) {
      break;
    }
    if (type == kMadtGenericInterrupt &&
        entry_length >= kMadtGiccMinimumLength) {
      const uint32_t flags =
        Read32(address + offset + kMadtGiccFlagsOffset);
      if ((flags & kMadtEnabled) != 0U) {
        ++g_info.cpu_count;
      }
      if (g_info.gic_redistributor == 0U) {
        const uint64_t gicr =
          Read64(address + offset + kMadtGiccGicrOffset);
        if (gicr != 0U) {
          g_info.gic_redistributor = gicr;
        }
      }
    } else if (type == kMadtGenericDistributor &&
               entry_length >= kMadtGicdMinimumLength) {
      const uint64_t gicd =
        Read64(address + offset + kMadtGicdBaseOffset);
      if (gicd != 0U) {
        g_info.gic_distributor = gicd;
      }
    }
    offset += entry_length;
  }
}

void ParseSpcr() noexcept {
  const Result<Table> table = FindTable("SPCR");
  if (table.is_err() || table.value().length < kSpcrMinimumLength) {
    return;
  }
  g_info.uart_base = Read64(table.value().address + kSpcrBaseOffset);
}

void ParseMcfg() noexcept {
  const Result<Table> table = FindTable("MCFG");
  if (table.is_err() || table.value().length < kMcfgMinimumLength) {
    return;
  }
  const uint64_t address = table.value().address;
  const uint64_t ecam = Read64(address + kMcfgFirstEntry);
  const uint8_t start_bus =
    Read8(address + kMcfgFirstEntry + kMcfgStartBusOffset);
  const uint8_t end_bus =
    Read8(address + kMcfgFirstEntry + kMcfgEndBusOffset);
  if (ecam == 0U || end_bus < start_bus) {
    return;
  }
  const uint32_t buses = static_cast<uint32_t>(end_bus - start_bus) + 1U;
  g_info.pci_ecam = ecam;
  g_info.pci_ecam_size = static_cast<uint64_t>(buses) << 20U;
}

}  // namespace

Status Init(uint64_t rsdp_address) noexcept {
  g_info = Info{};
  mem::Zero(g_tables, sizeof(g_tables));
  if (rsdp_address == 0U) {
    return Status::kNotFound;
  }
  if (!SignatureIs(rsdp_address, "RSD PTR ", 8U)) {
    return Status::kInvalidArg;
  }

  const uint8_t revision = Read8(rsdp_address + 15U);
  uint32_t rsdp_length = kRsdpChecksumLength;
  if (revision >= 2U) {
    rsdp_length = Read32(rsdp_address + 20U);
    if (rsdp_length < kRsdpV2Length || rsdp_length > 4096U) {
      return Status::kInvalidArg;
    }
  }
  if (!ChecksumOk(rsdp_address, kRsdpChecksumLength)) {
    return Status::kInvalidArg;
  }
  if (revision >= 2U && !ChecksumOk(rsdp_address, rsdp_length)) {
    return Status::kInvalidArg;
  }

  uint64_t root = 0;
  if (revision >= 2U && rsdp_length >= kRsdpV2Length) {
    root = Read64(rsdp_address + 24U);
  }
  if (root == 0U) {
    root = Read32(rsdp_address + 16U);
  }
  if (root == 0U) {
    return Status::kNotFound;
  }

  const uint32_t root_length = Read32(root + 4U);
  if (root_length < kSdtHeaderLength || root_length > kMaxTableLength) {
    return Status::kInvalidArg;
  }
  if (!ChecksumOk(root, root_length)) {
    return Status::kInvalidArg;
  }
  const bool extended = SignatureIs(root, "XSDT", kSignatureLength);
  if (!extended && !SignatureIs(root, "RSDT", kSignatureLength)) {
    return Status::kInvalidArg;
  }

  const uint32_t entry_size = extended ? 8U : 4U;
  uint32_t entries = (root_length - kSdtHeaderLength) / entry_size;
  if (entries > kMaxRootEntries) {
    entries = kMaxRootEntries;
  }
  for (uint32_t index = 0; index < entries; ++index) {
    const uint64_t entry_address =
      root + kSdtHeaderLength + (static_cast<uint64_t>(index) * entry_size);
    const uint64_t table_address =
      extended ? Read64(entry_address) : Read32(entry_address);
    if (table_address == 0U || g_info.table_count >= kMaxTables) {
      continue;
    }
    Table table{};
    if (!LoadTable(table_address, table)) {
      continue;
    }
    g_tables[g_info.table_count] = table;
    ++g_info.table_count;
  }

  g_info.ready = true;
  g_info.revision = revision;
  g_info.rsdp_address = rsdp_address;
  g_info.root_table_address = root;
  ParseMadt();
  ParseSpcr();
  ParseMcfg();
  g_info.has_gtdt = FindTable("GTDT").is_ok();
  return Status::kOk;
}

bool Ready() noexcept {
  return g_info.ready;
}

const Info& GetInfo() noexcept {
  return g_info;
}

Result<Table> TableAt(uint32_t index) noexcept {
  if (index >= g_info.table_count) {
    return Result<Table>::Err(Status::kNotFound);
  }
  return Result<Table>::Ok(g_tables[index]);
}

Result<Table> FindTable(const char* signature) noexcept {
  if (signature == nullptr) {
    return Result<Table>::Err(Status::kInvalidArg);
  }
  for (uint32_t index = 0; index < g_info.table_count; ++index) {
    if (mem::Equal(g_tables[index].signature, signature, kSignatureLength)) {
      return Result<Table>::Ok(g_tables[index]);
    }
  }
  return Result<Table>::Err(Status::kNotFound);
}

}  // namespace og::acpi
