// Host unit tests for pure libs/core code. No kernel, no hardware.

#include <cstdio>
#include <span>
#include <vector>

#include "bit.h"
#include "endian.h"
#include "format.h"
#include "memory.h"
#include "og/fs/partition.h"
#include "og/fs/vfs.h"
#include "og/platform/fdt.h"
#include "og/process/elf.h"

namespace {

int g_failures = 0;

void Check(bool passed, const char* expression, int line) {
  if (passed) {
    return;
  }
  std::printf("FAIL line %d: %s\n", line, expression);
  ++g_failures;
}

#define CHECK(expr) Check((expr), #expr, __LINE__)

void TestMemory() {
  char buffer[16];
  og::mem::Fill(buffer, 'A', sizeof(buffer));
  CHECK(buffer[0] == 'A');
  CHECK(buffer[15] == 'A');

  og::mem::Zero(buffer, sizeof(buffer));
  CHECK(buffer[0] == 0);
  CHECK(buffer[15] == 0);

  CHECK(og::mem::Length("hello") == 5);
  CHECK(og::mem::Length("") == 0);
  CHECK(og::mem::Same("abc", "abc"));
  CHECK(!og::mem::Same("abc", "abd"));

  CHECK(memcmp("abc", "abc", 3) == 0);
  CHECK(memcmp("abc", "abd", 3) < 0);
  CHECK(memcmp("abd", "abc", 3) > 0);

  char text[8] = "abcdefg";
  og::mem::Move(text + 1, text, 6);
  CHECK(text[0] == 'a');
  CHECK(text[1] == 'a');
  CHECK(text[6] == 'f');
  CHECK(text[7] == '\0');
}

void TestFormat() {
  char line[64];

  og::fmt::LineBuilder decimal(line, sizeof(line));
  decimal.Text("value").Text(": ").Dec(12345);
  CHECK(og::mem::Same(line, "value: 12345"));

  og::fmt::LineBuilder zero(line, sizeof(line));
  zero.Dec(0);
  CHECK(og::mem::Same(line, "0"));

  og::fmt::LineBuilder hex(line, sizeof(line));
  hex.Hex(0x1A2B);
  CHECK(og::mem::Same(line, "0x0000000000001a2b"));

  og::fmt::LineBuilder full(line, sizeof(line));
  full.Hex(0xFFFFFFFFFFFFFFFFULL);
  CHECK(og::mem::Same(line, "0xffffffffffffffff"));

  char small[4];
  og::fmt::LineBuilder tiny(small, sizeof(small));
  tiny.Text("abcdef");
  CHECK(og::mem::Length(small) == 3);
  CHECK(og::mem::Same(small, "abc"));
}

void TestBits() {
  static_assert(og::bits::Mask<uint32_t>(4) == 0xFU);
  static_assert(og::bits::Mask<uint32_t>(0) == 0U);
  static_assert(og::bits::Mask<uint32_t>(32) == 0xFFFFFFFFU);
  static_assert(og::bits::Extract<4, 4>(0xABU) == 0xAU);
  static_assert(og::bits::Insert<4, 4>(0x0FU, 0xAU) == 0xAFU);
  static_assert(og::bits::Bit<uint32_t>(3) == 8U);
  static_assert(og::bits::AlignUp<uint64_t>(0x1001, 0x1000) == 0x2000);
  static_assert(og::bits::AlignDown<uint64_t>(0x1FFF, 0x1000) == 0x1000);
  static_assert(og::bits::IsAligned<uint64_t>(0x2000, 0x1000));
  static_assert(!og::bits::IsAligned<uint64_t>(0x2001, 0x1000));

  CHECK((og::bits::Extract<0, 8>(0x1234U) == 0x34U));
  CHECK(og::bits::AlignUp<uint64_t>(1, 4096) == 4096);
}

void TestEndian() {
  const uint8_t data[] = {0x78, 0x56, 0x34, 0x12, 0xEF, 0xCD, 0xAB, 0x90};
  CHECK(og::endian::ReadLe16(data) == 0x5678U);
  CHECK(og::endian::ReadLe32(data) == 0x12345678U);
  CHECK(og::endian::ReadLe64(data) == 0x90ABCDEF12345678ULL);
}

void PutLe32(uint8_t* data, uint32_t value) {
  for (uint32_t index = 0; index < 4; ++index) {
    data[index] = static_cast<uint8_t>(value >> (index * 8U));
  }
}

void PutLe64(uint8_t* data, uint64_t value) {
  for (uint32_t index = 0; index < 8; ++index) {
    data[index] = static_cast<uint8_t>(value >> (index * 8U));
  }
}

struct FakeDisk {
  const uint8_t* data;
  uint64_t sectors;
};

og::Status ReadFakeDisk(void* context, uint64_t sector,
                        std::span<uint8_t> buffer) {
  auto* disk = static_cast<FakeDisk*>(context);
  const uint64_t offset = sector * 512U;
  if (disk == nullptr || offset + buffer.size() > disk->sectors * 512U) {
    return og::Status::kIoError;
  }
  for (std::size_t index = 0; index < buffer.size(); ++index) {
    buffer[index] = disk->data[offset + index];
  }
  return og::Status::kOk;
}

void TestGpt() {
  constexpr uint64_t kSectors = 16384;
  std::vector<uint8_t> image(kSectors * 512U, 0);

  uint8_t* header = image.data() + 512U;
  og::mem::Copy(header, "EFI PART", 8);
  PutLe64(header + 72U, 2);
  PutLe32(header + 80U, 128);
  PutLe32(header + 84U, 128);

  uint8_t* entries = image.data() + (2U * 512U);
  const uint8_t esp_guid[16] = {0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11,
                                0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B};
  og::mem::Copy(entries, esp_guid, sizeof(esp_guid));
  PutLe64(entries + 32U, 2048);
  PutLe64(entries + 40U, 4095);

  uint8_t* system = entries + 128U;
  for (uint32_t index = 0; index < 16U; ++index) {
    system[index] = static_cast<uint8_t>(0xA0U + index);
  }
  PutLe64(system + 32U, 4096);
  PutLe64(system + 40U, 6143);
  og::mem::Copy(image.data() + (4096U * 512U), "GSLFS1\0\0", 8);

  FakeDisk disk{image.data(), kSectors};
  const og::fs::BlockDevice device{&ReadFakeDisk, &disk, kSectors, 0};

  const auto esp = og::fs::FindEspPartition(device);
  CHECK(esp.is_ok());
  CHECK(esp.value().start_sector == 2048U);
  CHECK(esp.value().sector_count == 2048U);

  const uint8_t magic[8] = {'G', 'S', 'L', 'F', 'S', '1', '\0', '\0'};
  const auto gslfs = og::fs::FindPartitionByMagic(device, magic, sizeof(magic));
  CHECK(gslfs.is_ok());
  CHECK(gslfs.value().start_sector == 4096U);
  CHECK(gslfs.value().sector_count == 2048U);

  // A disk without the GPT signature is reported as no device, not a
  // malformed table.
  const uint8_t saved_signature = image[512U];
  image[512U] = 'X';
  CHECK(og::fs::FindEspPartition(device).status() == og::Status::kNoDevice);
  image[512U] = saved_signature;

  const uint8_t unknown[8] = {'N', 'O', 'P', 'E', '0', '0', '0', '0'};
  CHECK(og::fs::FindPartitionByMagic(device, unknown, sizeof(unknown)).status() ==
        og::Status::kNotFound);
  CHECK(og::fs::FindPartitionByMagic(device, nullptr, 0).status() ==
        og::Status::kInvalidArg);

  const og::fs::BlockDevice no_read{nullptr, nullptr, 0, 0};
  CHECK(og::fs::FindEspPartition(no_read).status() ==
        og::Status::kInvalidArg);

  const og::fs::BlockDevice tiny{&ReadFakeDisk, &disk, 1, 0};
  CHECK(og::fs::FindEspPartition(tiny).status() ==
        og::Status::kInvalidArg);

  std::vector<uint8_t> broken = image;
  PutLe32(broken.data() + 512U + 80U, 0U);
  FakeDisk broken_disk{broken.data(), kSectors};
  const og::fs::BlockDevice broken_device{&ReadFakeDisk, &broken_disk, kSectors,
                                          0};
  CHECK(og::fs::FindEspPartition(broken_device).status() ==
        og::Status::kInvalidArg);
}

void PutBe32(uint8_t* data, uint32_t value) {
  for (uint32_t index = 0; index < 4; ++index) {
    data[index] = static_cast<uint8_t>(value >> ((3U - index) * 8U));
  }
}

void TestFdt() {
  constexpr uint32_t kStructOffset = 56;
  constexpr uint32_t kStringsOffset = 108;
  constexpr uint32_t kStructSize = 52;
  constexpr uint32_t kStringsSize = 11;
  constexpr uint32_t kTotal = 119;

  std::vector<uint8_t> dtb(128, 0);
  PutBe32(dtb.data(), 0xD00DFEEDU);
  PutBe32(dtb.data() + 4U, kTotal);
  PutBe32(dtb.data() + 8U, kStructOffset);
  PutBe32(dtb.data() + 12U, kStringsOffset);
  PutBe32(dtb.data() + 16U, 40U);
  PutBe32(dtb.data() + 20U, 17U);
  PutBe32(dtb.data() + 24U, 16U);
  PutBe32(dtb.data() + 32U, kStringsSize);
  PutBe32(dtb.data() + 36U, kStructSize);

  uint8_t* node = dtb.data() + kStructOffset;
  PutBe32(node, 1U);
  og::mem::Copy(node + 4U, "root", 5);
  node += 12U;
  PutBe32(node, 3U);
  PutBe32(node + 4U, 4U);
  PutBe32(node + 8U, 0U);
  og::mem::Copy(node + 12U, "test", 4);
  node += 16U;
  PutBe32(node, 1U);
  og::mem::Copy(node + 4U, "child", 6);
  node += 12U;
  PutBe32(node, 2U);
  node += 4U;
  PutBe32(node, 2U);
  node += 4U;
  PutBe32(node, 9U);
  og::mem::Copy(dtb.data() + kStringsOffset, "compatible", 11);

  CHECK(og::IsOk(og::platform::fdt::Init(
    reinterpret_cast<uint64_t>(dtb.data()), dtb.size())));
  CHECK(og::platform::fdt::Ready());

  const auto root = og::platform::fdt::FindNode("/");
  CHECK(root.is_ok());
  const auto child = og::platform::fdt::FindNode("/child");
  CHECK(child.is_ok());
  CHECK(og::platform::fdt::ChildCount(root.value()) == 1U);
  const auto first_child = og::platform::fdt::Child(root.value(), 0);
  CHECK(first_child.is_ok());
  CHECK(first_child.value() == child.value());
  CHECK(!og::platform::fdt::Child(root.value(), 1).is_ok());

  const auto name = og::platform::fdt::NodeName(root.value());
  CHECK(name.is_ok());
  CHECK(og::mem::Same(name.value(), "root"));
  const auto child_name = og::platform::fdt::NodeName(child.value());
  CHECK(child_name.is_ok());
  CHECK(og::mem::Same(child_name.value(), "child"));

  const auto property =
    og::platform::fdt::FindProperty(root.value(), "compatible");
  CHECK(property.is_ok());
  CHECK(property.value().size() == 4U);
  CHECK(property.value()[0] == 't');
  CHECK(!og::platform::fdt::FindProperty(root.value(), "missing").is_ok());
  CHECK(!og::platform::fdt::FindNode("/missing").is_ok());
}

og::fs::Node g_root_node;
og::fs::Node g_dev_node;
og::fs::Node g_console_node;
og::fs::Node g_file_node;

og::Status RootLookup(og::fs::Node* /*directory*/, const char* name,
                      og::fs::Node** out) noexcept {
  if (og::mem::Same(name, "dev")) {
    *out = &g_dev_node;
    return og::Status::kOk;
  }
  if (og::mem::Same(name, "file")) {
    *out = &g_file_node;
    return og::Status::kOk;
  }
  return og::Status::kNotFound;
}

og::Status DevLookup(og::fs::Node* /*directory*/, const char* name,
                     og::fs::Node** out) noexcept {
  if (og::mem::Same(name, "console")) {
    *out = &g_console_node;
    return og::Status::kOk;
  }
  return og::Status::kNotFound;
}

og::Status RootIterate(og::fs::Node* /*directory*/, uint32_t index,
                       og::fs::DirEntry& entry, bool& found) noexcept {
  found = false;
  if (index > 1U) {
    return og::Status::kOk;
  }
  og::mem::Zero(&entry, sizeof(entry));
  if (index == 0U) {
    og::mem::Copy(entry.name, "dev", 4);
    entry.type = og::fs::NodeType::kDirectory;
  } else {
    og::mem::Copy(entry.name, "file", 5);
    entry.type = og::fs::NodeType::kFile;
    entry.size = 16;
  }
  found = true;
  return og::Status::kOk;
}

const og::fs::NodeOps kRootOps{&RootLookup, nullptr, nullptr, &RootIterate};
const og::fs::NodeOps kDevOps{&DevLookup, nullptr, nullptr, nullptr};

// Exactly 16 payload bytes: the virtual file has no terminating NUL.
char g_file_data[32] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
                        'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'};

og::Status FileReadOp(og::fs::Node* node, uint64_t offset,
                      std::span<uint8_t> buffer, uint64_t& out_read) noexcept {
  out_read = 0;
  if (node == nullptr || buffer.empty()) {
    return og::Status::kInvalidArg;
  }
  if (offset >= node->size) {
    return og::Status::kOk;
  }
  uint64_t available = node->size - offset;
  if (available > buffer.size()) {
    available = buffer.size();
  }
  for (uint64_t index = 0; index < available; ++index) {
    buffer[index] = static_cast<uint8_t>(g_file_data[offset + index]);
  }
  out_read = available;
  return og::Status::kOk;
}

og::Status FileWriteOp(og::fs::Node* node, uint64_t offset,
                       std::span<const uint8_t> buffer,
                       uint64_t& out_written) noexcept {
  out_written = 0;
  if (node == nullptr || buffer.empty() || offset >= sizeof(g_file_data)) {
    return og::Status::kInvalidArg;
  }
  uint64_t available = sizeof(g_file_data) - offset;
  if (available > buffer.size()) {
    available = buffer.size();
  }
  for (uint64_t index = 0; index < available; ++index) {
    g_file_data[offset + index] = static_cast<char>(buffer[index]);
  }
  if (offset + available > node->size) {
    node->size = offset + available;
  }
  out_written = available;
  return og::Status::kOk;
}

const og::fs::NodeOps kFileOps{nullptr, &FileReadOp, &FileWriteOp, nullptr};

void TestVfs() {
  g_root_node = og::fs::Node{"/", og::fs::NodeType::kDirectory, 0, &kRootOps,
                             nullptr, nullptr, 0, 0, 0, 0};
  g_dev_node = og::fs::Node{"dev", og::fs::NodeType::kDirectory, 0, &kDevOps,
                            nullptr, nullptr, 0, 0, 0, 0};
  g_console_node = og::fs::Node{"console", og::fs::NodeType::kDevice, 0,
                                nullptr, nullptr, nullptr, 0, 0, 0, 0};
  g_file_node = og::fs::Node{"file", og::fs::NodeType::kFile, 12, nullptr,
                             nullptr, nullptr, 0, 0, 0, 0};

  CHECK(og::IsOk(og::fs::FsInit()));
  CHECK(og::IsOk(og::fs::FsMount("/", &g_root_node)));
  CHECK(og::IsOk(og::fs::FsMount("/dev", &g_dev_node)));
  CHECK(!og::IsOk(og::fs::FsMount("/dev", &g_dev_node)));

  const auto root = og::fs::FsResolve("/");
  CHECK(root.is_ok());
  CHECK(root.value() == &g_root_node);

  const auto dev = og::fs::FsResolve("/dev");
  CHECK(dev.is_ok());
  CHECK(dev.value() == &g_dev_node);

  const auto console = og::fs::FsResolve("/dev/console");
  CHECK(console.is_ok());
  CHECK(console.value() == &g_console_node);

  const auto file = og::fs::FsResolve("/file");
  CHECK(file.is_ok());
  CHECK(file.value() == &g_file_node);

  CHECK(!og::fs::FsResolve("/missing").is_ok());
  CHECK(!og::fs::FsResolve("/../file").is_ok());
  CHECK(!og::fs::FsResolve("/dev/../file").is_ok());

  const auto stat = og::fs::FsStat("/dev/console");
  CHECK(stat.is_ok());
  CHECK(stat.value().type == og::fs::NodeType::kDevice);

  CHECK(og::fs::FsResolve(nullptr).status() == og::Status::kInvalidArg);
  CHECK(og::fs::FsStat("/missing").status() == og::Status::kNotFound);
  CHECK(og::fs::FsMount("/bad", &g_file_node) == og::Status::kInvalidArg);

  og::fs::DirEntry entry{};
  bool found = false;
  CHECK(og::IsOk(og::fs::FsList("/", 0, entry, found)));
  CHECK(found);
  CHECK(og::mem::Same(entry.name, "dev"));
  CHECK(entry.type == og::fs::NodeType::kDirectory);
  CHECK(og::IsOk(og::fs::FsList("/", 1, entry, found)));
  CHECK(found);
  CHECK(og::mem::Same(entry.name, "file"));
  CHECK(entry.type == og::fs::NodeType::kFile);
  CHECK(entry.size == 16U);
  CHECK(og::IsOk(og::fs::FsList("/", 2, entry, found)));
  CHECK(!found);
  CHECK(og::fs::FsList("/file", 0, entry, found) ==
        og::Status::kInvalidArg);
  CHECK(og::fs::FsList("/dev", 0, entry, found) ==
        og::Status::kUnsupported);
  CHECK(og::fs::FsList("/missing", 0, entry, found) ==
        og::Status::kNotFound);

  constexpr uint64_t kInitialFileSize = 16;
  og::mem::Copy(g_file_data, "abcdefghijklmnop", kInitialFileSize);
  g_file_node.size = kInitialFileSize;
  g_file_node.ops = &kFileOps;

  og::fs::FileTable table{};
  CHECK(og::IsOk(og::fs::FileTableInit(table)));
  const auto descriptor = og::fs::FsOpen(table, "/file", og::fs::kOpenWrite);
  CHECK(descriptor.is_ok());
  CHECK(descriptor.value() == 3U);

  char buffer[4]{};
  uint64_t read = 0;
  CHECK(og::IsOk(og::fs::FsRead(
    table, descriptor.value(), std::span<uint8_t>(
      reinterpret_cast<uint8_t*>(buffer), sizeof(buffer)), read)));
  CHECK(read == 4U);
  CHECK(buffer[0] == 'a' && buffer[3] == 'd');

  CHECK(og::IsOk(og::fs::FsSeek(table, descriptor.value(), 0)));
  const char payload[] = "XY";
  uint64_t written = 0;
  CHECK(og::IsOk(og::fs::FsWrite(
    table, descriptor.value(),
    std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(payload), 2),
    written)));
  CHECK(written == 2U);
  CHECK(g_file_data[0] == 'X' && g_file_data[1] == 'Y');

  CHECK(og::IsOk(og::fs::FsSeek(table, descriptor.value(), 12)));
  char tail[4]{};
  uint64_t tail_read = 0;
  CHECK(og::IsOk(og::fs::FsRead(
    table, descriptor.value(),
    std::span<uint8_t>(reinterpret_cast<uint8_t*>(tail), sizeof(tail)),
    tail_read)));
  CHECK(tail_read == 4U);
  CHECK(tail[0] == 'm' && tail[3] == 'p');

  CHECK(og::IsOk(og::fs::FsSeek(table, descriptor.value(), g_file_node.size)));
  uint64_t eof_read = 1;
  CHECK(og::IsOk(og::fs::FsRead(
    table, descriptor.value(),
    std::span<uint8_t>(reinterpret_cast<uint8_t*>(tail), sizeof(tail)),
    eof_read)));
  CHECK(eof_read == 0U);

  const auto second = og::fs::FsOpen(table, "/file");
  CHECK(second.is_ok());
  CHECK(second.value() == 4U);
  CHECK(og::IsOk(og::fs::FsClose(table, second.value())));

  // Closing a descriptor frees it for the next open.
  const auto reused = og::fs::FsOpen(table, "/file");
  CHECK(reused.is_ok());
  CHECK(reused.value() == 4U);
  CHECK(og::IsOk(og::fs::FsClose(table, reused.value())));

  // A read-only descriptor cannot write.
  const auto readonly = og::fs::FsOpen(table, "/file");
  CHECK(readonly.is_ok());
  const char denied = 'Q';
  uint64_t denied_written = 0;
  CHECK(og::fs::FsWrite(
          table, readonly.value(),
          std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(&denied), 1),
          denied_written) == og::Status::kUnsupported);
  CHECK(og::IsOk(og::fs::FsClose(table, readonly.value())));

  // Append writes at the current end even when the descriptor offset is zero.
  const auto appended = og::fs::FsOpen(
    table, "/file", og::fs::kOpenWrite | og::fs::kOpenAppend);
  CHECK(appended.is_ok());
  const char suffix[] = "Z!";
  uint64_t suffix_written = 0;
  CHECK(og::IsOk(og::fs::FsWrite(
    table, appended.value(),
    std::span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(suffix), 2),
    suffix_written)));
  CHECK(suffix_written == 2U);
  CHECK(g_file_node.size == 18U);
  CHECK(g_file_data[16] == 'Z' && g_file_data[17] == '!');
  CHECK(og::IsOk(og::fs::FsSeek(table, appended.value(), 16)));
  char suffix_read[2]{};
  uint64_t suffix_read_count = 0;
  CHECK(og::IsOk(og::fs::FsRead(
    table, appended.value(),
    std::span<uint8_t>(reinterpret_cast<uint8_t*>(suffix_read), 2),
    suffix_read_count)));
  CHECK(suffix_read_count == 2U);
  CHECK(suffix_read[0] == 'Z' && suffix_read[1] == '!');
  CHECK(og::IsOk(og::fs::FsClose(table, appended.value())));

  CHECK(og::fs::FsSeek(table, descriptor.value(), g_file_node.size + 1U) ==
        og::Status::kInvalidArg);
  CHECK(og::IsOk(og::fs::FsClose(table, descriptor.value())));
  CHECK(og::fs::FsRead(
          table, descriptor.value(),
          std::span<uint8_t>(reinterpret_cast<uint8_t*>(buffer), sizeof(buffer)),
          read) == og::Status::kNotFound);
  CHECK(!og::fs::FsOpen(table, "/missing").is_ok());
}

void PutLe16(uint8_t* data, uint16_t value) {
  data[0] = static_cast<uint8_t>(value);
  data[1] = static_cast<uint8_t>(value >> 8U);
}

void BuildMinimalElf(std::vector<uint8_t>& header) {
  header.assign(1024, 0);
  header[0] = 0x7FU;
  header[1] = 'E';
  header[2] = 'L';
  header[3] = 'F';
  header[4] = 2U;
  header[5] = 1U;
  header[6] = 1U;
  PutLe16(header.data() + 16U, 2U);
  PutLe16(header.data() + 18U, 183U);
  PutLe32(header.data() + 20U, 1U);
  PutLe64(header.data() + 24U, 0x10000000000ULL);
  PutLe64(header.data() + 32U, 64U);
  PutLe16(header.data() + 52U, 64U);
  PutLe16(header.data() + 54U, 56U);
  PutLe16(header.data() + 56U, 1U);

  uint8_t* phdr = header.data() + 64U;
  PutLe32(phdr, 1U);
  PutLe32(phdr + 4U, 1U);
  PutLe64(phdr + 8U, 0x1000U);
  PutLe64(phdr + 16U, 0x10000000000ULL);
  PutLe64(phdr + 32U, 0x100U);
  PutLe64(phdr + 40U, 0x100U);
  PutLe64(phdr + 48U, 0x1000U);
}

void TestElf() {
  std::vector<uint8_t> header;
  BuildMinimalElf(header);
  const auto valid = og::process::elf::Parse(
    std::span<const uint8_t>(header.data(), header.size()), 0x2000U);
  CHECK(valid.is_ok());
  CHECK(valid.value().entry == 0x10000000000ULL);
  CHECK(valid.value().dynamic_address == 0U);
  CHECK(valid.value().interpreter_size == 0U);
  CHECK(valid.value().segment_count == 1U);

  header[1] = 'X';
  CHECK(!og::process::elf::Parse(
    std::span<const uint8_t>(header.data(), header.size()), 0x2000U).is_ok());

  header[1] = 'E';
  PutLe16(header.data() + 56U, 0U);
  CHECK(!og::process::elf::Parse(
    std::span<const uint8_t>(header.data(), header.size()), 0x2000U).is_ok());

  BuildMinimalElf(header);
  PutLe64(header.data() + 64U + 16U, 0x1000U);
  CHECK(!og::process::elf::Parse(
    std::span<const uint8_t>(header.data(), header.size()), 0x2000U).is_ok());

  BuildMinimalElf(header);
  PutLe64(header.data() + 24U, 0x2000U);
  CHECK(!og::process::elf::Parse(
    std::span<const uint8_t>(header.data(), header.size()), 0x2000U).is_ok());
}

}  // namespace

int main() {
  TestMemory();
  TestFormat();
  TestBits();
  TestEndian();
  TestGpt();
  TestFdt();
  TestVfs();
  TestElf();

  if (g_failures != 0) {
    std::printf("host tests: %d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("host tests: all passed\n");
  return 0;
}
