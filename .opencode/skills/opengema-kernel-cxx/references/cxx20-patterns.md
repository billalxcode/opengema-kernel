# Pola C++20 Kernel

## Concepts

```cpp
template <typename T>
concept MmioWord = std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t>;
```

## Span untuk buffer

```cpp
Status Write(std::span<const uint8_t> buf);
```

Jika span kosong, return early `InvalidArg`. Jangan terima `(ptr, len)` terpisah.

## Error tanpa exceptions

```cpp
enum class Status : uint32_t { kOk = 0, kInvalidArg, kNotInit, kNoDevice, kNoMem };
template <typename T> using Expected = std::variant<T, Status>;
```

Semua fungsi fallible return `[[nodiscard]] Status` atau `Expected<T>`.

## RAII tanpa throw

`LockGuard`, `ScopeGuard` cleanup di destructor `noexcept`. Constructor tidak alokasi. Init yang bisa gagal dipisah ke `Init()`.

## Compile time

`consteval` untuk hitung atribut MAIR dan TCR. `static_assert` untuk `sizeof(PageTable) == 4096` dan `alignof(VectorTable) == 2048`.
