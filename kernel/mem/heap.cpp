#include "og/mem/heap.h"

#include "memory.h"
#include "og/arch/spinlock.h"
#include "og/mem/pmm.h"

namespace og::mem {

namespace {

constexpr uint64_t kSlabSizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048};
constexpr uint64_t kSlabClassCount = sizeof(kSlabSizes) / sizeof(kSlabSizes[0]);
constexpr uint32_t kLargeClass = 0xFFFFFFFFU;
constexpr uint64_t kHeapMagic = 0x4F4748454150ULL;  // "OGHEAP"

struct BlockHeader {
  uint64_t magic;
  uint32_t class_index;
  uint32_t page_count;
};

static_assert(sizeof(BlockHeader) == 16);

struct FreeNode {
  FreeNode* next;
};

FreeNode* g_free_lists[kSlabClassCount];
uint64_t g_pages = 0;
uint64_t g_allocations = 0;
uint64_t g_frees = 0;
bool g_ready = false;
arch::aarch64::IrqSpinlock g_lock;

[[nodiscard]] uint64_t ClassIndexFor(uint64_t size) noexcept {
  for (uint64_t index = 0; index < kSlabClassCount; ++index) {
    if (size <= kSlabSizes[index]) {
      return index;
    }
  }
  return kSlabClassCount;
}

[[nodiscard]] BlockHeader* HeaderOf(void* pointer) noexcept {
  auto* bytes = static_cast<uint8_t*>(pointer);
  return reinterpret_cast<BlockHeader*>(bytes - sizeof(BlockHeader));
}

[[nodiscard]] Status GrowClass(uint64_t class_index) noexcept {
  const Result<uint64_t> page = PmmAllocPage();
  if (page.is_err()) {
    return page.status();
  }
  const uint64_t object_size = sizeof(BlockHeader) + kSlabSizes[class_index];
  if (object_size > kPageSize) {
    return Status::kInvalidArg;
  }
  const uint64_t count = kPageSize / object_size;
  auto* base = reinterpret_cast<uint8_t*>(page.value());
  for (uint64_t index = 0; index < count; ++index) {
    auto* node = reinterpret_cast<FreeNode*>(base + (index * object_size));
    node->next = g_free_lists[class_index];
    g_free_lists[class_index] = node;
  }
  ++g_pages;
  return Status::kOk;
}

[[nodiscard]] Result<void*> AllocLarge(uint64_t size) noexcept {
  const uint64_t total = sizeof(BlockHeader) + size;
  if (total < size) {
    return Result<void*>::Err(Status::kInvalidArg);
  }
  const uint64_t pages = (total + kPageSize - 1U) / kPageSize;
  if (pages > 0xFFFFFFFFULL) {
    return Result<void*>::Err(Status::kInvalidArg);
  }
  const Result<uint64_t> address = PmmAllocPages(pages);
  if (address.is_err()) {
    return Result<void*>::Err(address.status());
  }
  auto* header = reinterpret_cast<BlockHeader*>(address.value());
  header->magic = kHeapMagic;
  header->class_index = kLargeClass;
  header->page_count = static_cast<uint32_t>(pages);
  g_pages += pages;
  ++g_allocations;
  return Result<void*>::Ok(reinterpret_cast<void*>(header + 1));
}

}  // namespace

Status HeapInit() noexcept {
  g_lock.lock();
  if (g_ready) {
    g_lock.unlock();
    return Status::kOk;
  }
  if (!PmmReady()) {
    g_lock.unlock();
    return Status::kNotInit;
  }
  for (uint64_t index = 0; index < kSlabClassCount; ++index) {
    g_free_lists[index] = nullptr;
  }
  g_pages = 0;
  g_allocations = 0;
  g_frees = 0;
  g_ready = true;
  g_lock.unlock();
  return Status::kOk;
}

Result<void*> AllocLocked(uint64_t size) noexcept {
  if (!g_ready) {
    return Result<void*>::Err(Status::kNotInit);
  }
  if (size == 0) {
    return Result<void*>::Err(Status::kInvalidArg);
  }

  const uint64_t class_index = ClassIndexFor(size);
  if (class_index >= kSlabClassCount) {
    return AllocLarge(size);
  }

  if (g_free_lists[class_index] == nullptr) {
    const Status status = GrowClass(class_index);
    if (IsErr(status)) {
      return Result<void*>::Err(status);
    }
  }

  FreeNode* node = g_free_lists[class_index];
  g_free_lists[class_index] = node->next;
  auto* header = reinterpret_cast<BlockHeader*>(node);
  header->magic = kHeapMagic;
  header->class_index = static_cast<uint32_t>(class_index);
  header->page_count = 0;
  ++g_allocations;
  return Result<void*>::Ok(reinterpret_cast<void*>(header + 1));
}

Result<void*> HeapZalloc(uint64_t size) noexcept {
  Result<void*> block = HeapAlloc(size);
  if (block.is_err()) {
    return block;
  }
  mem::Zero(block.value(), size);
  return block;
}

Status FreeLocked(void* pointer) noexcept {
  if (!g_ready) {
    return Status::kNotInit;
  }
  if (pointer == nullptr) {
    return Status::kInvalidArg;
  }

  BlockHeader* header = HeaderOf(pointer);
  if (header->magic != kHeapMagic) {
    return Status::kInvalidArg;
  }

  const uint32_t class_index = header->class_index;
  const uint32_t page_count = header->page_count;
  header->magic = 0;

  if (class_index == kLargeClass) {
    const Status status = PmmFreePages(reinterpret_cast<uint64_t>(header), page_count);
    if (IsErr(status)) {
      return status;
    }
    ++g_frees;
    return Status::kOk;
  }

  if (class_index >= kSlabClassCount) {
    return Status::kInvalidArg;
  }
  auto* node = reinterpret_cast<FreeNode*>(header);
  node->next = g_free_lists[class_index];
  g_free_lists[class_index] = node;
  ++g_frees;
  return Status::kOk;
}

Result<void*> ReallocLocked(void* pointer, uint64_t size) noexcept {
  if (!g_ready) {
    return Result<void*>::Err(Status::kNotInit);
  }
  if (pointer == nullptr) {
    return AllocLocked(size);
  }
  if (size == 0) {
    const Status status = FreeLocked(pointer);
    if (IsErr(status)) {
      return Result<void*>::Err(status);
    }
    return Result<void*>::Ok(nullptr);
  }

  BlockHeader* header = HeaderOf(pointer);
  if (header->magic != kHeapMagic) {
    return Result<void*>::Err(Status::kInvalidArg);
  }

  uint64_t old_size = 0;
  if (header->class_index == kLargeClass) {
    old_size = (static_cast<uint64_t>(header->page_count) * kPageSize) - sizeof(BlockHeader);
  } else {
    if (header->class_index >= kSlabClassCount) {
      return Result<void*>::Err(Status::kInvalidArg);
    }
    old_size = kSlabSizes[header->class_index];
  }

  // Keep the block when the new size still fits without wasting half of it.
  if (size <= old_size && size > (old_size / 2U)) {
    return Result<void*>::Ok(pointer);
  }

  Result<void*> fresh = AllocLocked(size);
  if (fresh.is_err()) {
    return fresh;
  }
  const uint64_t copy_size = size < old_size ? size : old_size;
  mem::Copy(fresh.value(), pointer, copy_size);
  const Status status = FreeLocked(pointer);
  if (IsErr(status)) {
    return Result<void*>::Err(status);
  }
  return fresh;
}

Result<void*> HeapAlloc(uint64_t size) noexcept {
  g_lock.lock();
  const Result<void*> block = AllocLocked(size);
  g_lock.unlock();
  return block;
}

Status HeapFree(void* pointer) noexcept {
  g_lock.lock();
  const Status status = FreeLocked(pointer);
  g_lock.unlock();
  return status;
}

Result<void*> HeapRealloc(void* pointer, uint64_t size) noexcept {
  g_lock.lock();
  const Result<void*> block = ReallocLocked(pointer, size);
  g_lock.unlock();
  return block;
}

HeapStats GetHeapStats() noexcept {
  HeapStats stats{};
  g_lock.lock();
  stats.pages = g_pages;
  stats.allocations = g_allocations;
  stats.frees = g_frees;
  g_lock.unlock();
  return stats;
}

}  // namespace og::mem
