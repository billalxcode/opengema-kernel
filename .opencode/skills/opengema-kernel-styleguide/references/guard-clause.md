# Guard Clause

Aturan: kondisi gagal dulu, return early, path utama tanpa indent.

```cpp
Status Map(uint64_t va, uint64_t pa) {
  if (!IsAligned(va)) {
    return Status::Unaligned();
  }
  if (!IsAligned(pa)) {
    return Status::Unaligned();
  }
  if (table_ == nullptr) {
    return Status::NotInit();
  }
  return MapInner(va, pa);
}
```

Larangan: nesting sukses dulu, else setelah return, validasi di tengah fungsi.
Maksimal 1 level nesting untuk path utama.
