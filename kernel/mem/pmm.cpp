#include "og/mem/pmm.h"

#include "memory.h"

namespace og::mem {

namespace {

constexpr uint64_t kBitmapWords = kMaxFrames / 64U;

uint64_t g_bitmap[kBitmapWords];
uint64_t g_ram_base = 0;
uint64_t g_frame_count = 0;
uint64_t g_free_count = 0;
uint64_t g_hint = 0;
bool g_ready = false;

[[nodiscard]] uint64_t BitMask(uint64_t index) noexcept {
  return 1ULL << (index & 63U);
}

[[nodiscard]] bool FrameIsFree(uint64_t index) noexcept {
  return (g_bitmap[index >> 6U] & BitMask(index)) == 0;
}

void MarkFrameFree(uint64_t index) noexcept {
  g_bitmap[index >> 6U] &= ~BitMask(index);
}

void MarkFrameUsed(uint64_t index) noexcept {
  g_bitmap[index >> 6U] |= BitMask(index);
}

// Releases every frame in [base, base + size) that falls inside RAM.
void ReleaseRange(uint64_t base, uint64_t size) noexcept {
  if (size == 0) {
    return;
  }
  const uint64_t ram_end = g_ram_base + (g_frame_count * kPageSize);
  const uint64_t end = base + size;
  if (end < base) {
    return;  // overflow
  }
  if (base >= ram_end) {
    return;
  }
  if (end <= g_ram_base) {
    return;
  }

  const uint64_t clamped_begin = base < g_ram_base ? g_ram_base : base;
  const uint64_t clamped_end = end > ram_end ? ram_end : end;
  const uint64_t first = (clamped_begin - g_ram_base) / kPageSize;
  const uint64_t last = (clamped_end - g_ram_base) / kPageSize;

  for (uint64_t index = first; index < last; ++index) {
    if (!FrameIsFree(index)) {
      MarkFrameFree(index);
      ++g_free_count;
    }
  }
}

// Finds count consecutive free frames in [begin, end). Does not modify state.
[[nodiscard]] bool ScanRange(uint64_t begin, uint64_t end, uint64_t count,
                            uint64_t& start_out) noexcept {
  uint64_t run = 0;
  uint64_t run_start = 0;
  for (uint64_t index = begin; index < end; ++index) {
    if (!FrameIsFree(index)) {
      run = 0;
      continue;
    }
    if (run == 0) {
      run_start = index;
    }
    ++run;
    if (run == count) {
      start_out = run_start;
      return true;
    }
  }
  return false;
}

}  // namespace

Status PmmInit(const BootInfo& info) noexcept {
  if (g_ready) {
    return Status::kOk;
  }
  if (info.ram_size == 0) {
    return Status::kInvalidArg;
  }
  if ((info.ram_base % kPageSize) != 0) {
    return Status::kUnaligned;
  }

  g_ram_base = info.ram_base;
  uint64_t frames = info.ram_size / kPageSize;
  if (frames > kMaxFrames) {
    frames = kMaxFrames;
  }
  if (frames == 0) {
    return Status::kNoMem;
  }
  g_frame_count = frames;
  g_free_count = 0;
  g_hint = 0;

  // Start with everything reserved, then release only ConventionalMemory.
  for (uint64_t word = 0; word < kBitmapWords; ++word) {
    g_bitmap[word] = ~0ULL;
  }

  for (uint64_t index = 0; index < info.memory_count; ++index) {
    const MemoryRange& range = info.memory[index];
    if (range.type != MemoryType::kUsable) {
      continue;
    }
    ReleaseRange(range.base, range.size);
  }

  if (g_free_count == 0) {
    return Status::kNoMem;
  }

  g_ready = true;
  return Status::kOk;
}

Result<uint64_t> PmmAllocPages(uint64_t count) noexcept {
  if (!g_ready) {
    return Result<uint64_t>::Err(Status::kNotInit);
  }
  if (count == 0) {
    return Result<uint64_t>::Err(Status::kInvalidArg);
  }
  if (count > g_frame_count) {
    return Result<uint64_t>::Err(Status::kNoMem);
  }
  if (count > g_free_count) {
    return Result<uint64_t>::Err(Status::kNoMem);
  }

  uint64_t start = 0;
  bool found = ScanRange(g_hint, g_frame_count, count, start);
  if (!found) {
    found = ScanRange(0, g_hint, count, start);
  }
  if (!found) {
    return Result<uint64_t>::Err(Status::kNoMem);
  }

  for (uint64_t offset = 0; offset < count; ++offset) {
    MarkFrameUsed(start + offset);
  }
  g_free_count -= count;
  g_hint = start + count;
  if (g_hint >= g_frame_count) {
    g_hint = 0;
  }

  return Result<uint64_t>::Ok(g_ram_base + (start * kPageSize));
}

Result<uint64_t> PmmAllocPage() noexcept {
  return PmmAllocPages(1);
}

Status PmmFreePages(uint64_t address, uint64_t count) noexcept {
  if (!g_ready) {
    return Status::kNotInit;
  }
  if (count == 0) {
    return Status::kInvalidArg;
  }
  if ((address % kPageSize) != 0) {
    return Status::kUnaligned;
  }
  if (address < g_ram_base) {
    return Status::kInvalidArg;
  }

  const uint64_t offset = (address - g_ram_base) / kPageSize;
  if (offset >= g_frame_count) {
    return Status::kInvalidArg;
  }
  if (count > (g_frame_count - offset)) {
    return Status::kInvalidArg;
  }

  // Validate first so a bad request does not free half a range.
  for (uint64_t step = 0; step < count; ++step) {
    if (FrameIsFree(offset + step)) {
      return Status::kInvalidArg;
    }
  }
  for (uint64_t step = 0; step < count; ++step) {
    MarkFrameFree(offset + step);
  }
  g_free_count += count;
  if (offset < g_hint) {
    g_hint = offset;
  }
  return Status::kOk;
}

Status PmmFreePage(uint64_t address) noexcept {
  return PmmFreePages(address, 1);
}

uint64_t PmmFreeFrames() noexcept {
  return g_free_count;
}

uint64_t PmmTotalFrames() noexcept {
  return g_frame_count;
}

bool PmmReady() noexcept {
  return g_ready;
}

}  // namespace og::mem
