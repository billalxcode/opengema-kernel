---
name: opengema-kernel-cxx
description: Senior C++20 freestanding untuk kernel ARM64 opengema. Dipakai saat menulis atau review kode kernel.
---

# opengema-kernel-cxx

Target: QEMU virt ARM64, UEFI boot, C++20 freestanding, clang + lld.

## 1. Guard clause dulu

Utamakan kondisi gagal di awal. Jangan nesting dalam.

```cpp
// Benar
Status Uart::Write(std::span<const uint8_t> buf) {
  if (!is_init_) {
    return Status::NotInit();
  }
  if (buf.empty()) {
    return Status::InvalidArg();
  }
  if (!mmio_) {
    return Status::NoDevice();
  }
  // path utama di sini, tanpa indent tambahan
  return WritePoll(buf);
}

// Salah: nesting sukses dulu
```

Aturan: validasi argumen, state, dan pointer di 3-5 baris pertama. Return early.

## 2. Batasan freestanding

Flags wajib:

```
-ffreestanding -nostdlib -fno-exceptions -fno-rtti
-fno-threadsafe-statics -fno-unwind-tables
-Wall -Wextra -Werror -Wconversion -Wpedantic
```

Konsekuensi:

- Jika butuh throw, hentikan. Gunakan `Status`, `Expected<T>`, atau panic.
- Jika butuh `dynamic_cast` atau `typeid`, hentikan. Gunakan `enum class` + `variant` atau manual tag.
- Jika butuh `new` yang throw, hentikan. Sediakan `operator new` placement + `nothrow` sendiri.
- Jika butuh iostream, vector, string alokasi, hentikan. Hanya header freestanding yang boleh.

Header yang aman di kernel:

```
<cstdint> <cstddef> <span> <array> <string_view>
<concepts> <type_traits> <utility> <optional> <variant>
<atomic> <bit> <limits> <algorithm> (constexpr only)
```

Header yang dilarang: `<iostream> <vector> <string> <map> <format>` dengan alokasi, `<stdexcept> <exception> <typeinfo> <filesystem> <thread> <future>`.

Sumber: P2338R4 Freestanding Library, OSDev C++ Bare Bones, C++ Core Guidelines P.2.

## 3. Pola C++20 yang diwajibkan

- `concepts` untuk MMIO dan driver:

```cpp
template <typename T>
concept MmioWord = std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t>;
```

- `constexpr`, `consteval`, `static_assert` untuk tabel dan layout:

```cpp
static_assert(sizeof(PageTable) == 4096);
consteval uint64_t MaIndex(DeviceMem) { return 0; }
```

- `std::span` untuk buffer, jangan `(ptr, len)` terpisah:

```cpp
void Read(std::span<uint8_t> out);
```

- `enum class` + `std::variant` untuk status dan error, bukan int magis.
- RAII tanpa exceptions: `ScopeGuard`, `LockGuard`, cleanup di destructor. Destructor harus `noexcept`.
- `[[nodiscard]]` untuk semua fungsi yang return `Status` atau pointer.
- `noexcept` untuk semua kode EL1 panas (trap, MMU, scheduler).

## 4. Aturan ARM64 keras

- MMIO selalu lewat `std::atomic_ref` atau `volatile` terisolasi + barrier. Jangan optimasi manual.

```cpp
inline void MmioWrite32(uint64_t addr, uint32_t v) {
  auto* p = reinterpret_cast<volatile uint32_t*>(addr);
  *p = v;
  __asm__ volatile("dsb sy" ::: "memory");
}
```

- Barrier: `dsb sy` setelah config device, `isb` setelah tulis `VBAR_EL1`, `TTBR`, `SCTLR`, `dsb+isb` sebelum `eret`.
- Alignment: `struct alignas(4096)`, vector table `alignas(2048)`. Cek dengan `static_assert(alignof)`.
- Jangan pakai `reinterpret_cast` liar. Bungkus dalam `MmioRegion` dengan bounds check.
- Endian dan ukuran: pakai `uint32_t`/`uint64_t` eksplisit, jangan `long` atau `int`.
- ESR_EL1, FAR_EL1, ELR_EL1 decode harus struct bitfield yang diuji, bukan shift magis tersebar.

## 5. Larangan senior

- Dilarang `using namespace` di header.
- Dilarang macro untuk logika. Macro hanya untuk linker symbol dan assembly glue.
- Dilarang recursion tak terbatas di EL1 stack 16K.
- Dilarang floating point di kernel tanpa save/restore SIMD eksplisit.
- Dilarang `reinterpret_cast` tanpa `static_assert` ukuran dan alignment.
- Dilarang komentar panjang sebagai pengganti tipe yang jelas. Tipe harus ungkapkan intent (Core Guidelines P.1, P.3).

## 6. Checklist review

1. Guard clause ada di awal?
2. Return type `[[nodiscard]]`?
3. Ada `new/delete` telanjang? Ganti dengan owner eksplisit.
4. Ada exception/RTTI/iostream? Tolak.
5. MMIO ada barrier?
6. Semua magic address dari `memory_map.h`, bukan literal di .cpp?

## References

- Baca `references/freestanding.md` untuk batasan freestanding.
- Baca `references/mmio-barriers.md` untuk MMIO dan barrier.
- Baca `references/cxx20-patterns.md` untuk pola concepts, span, dan Expected.
- Baca `references/toolchain.md` untuk alasan lld-link + header sendiri.
