# MMIO dan Barrier ARM64

QEMU virt: UART0 `0x09000000`, GICD `0x08000000`, RAM `0x40000000`.

## Pola tulis

```cpp
inline void MmioWrite32(uint64_t addr, uint32_t v) {
  auto* p = reinterpret_cast<volatile uint32_t*>(addr);
  if (p == nullptr) {
    return;
  }
  *p = v;
  __asm__ volatile("dsb sy" ::: "memory");
}
```

## Aturan barrier

- `dsb sy` setelah tulis device dan sebelum baca status.
- `isb` setelah tulis `VBAR_EL1`, `TTBR0_EL1`, `TTBR1_EL1`, `TCR_EL1`, `SCTLR_EL1`.
- `dsb sy; isb` sebelum `eret` dari trap init.
- MMIO bungkus di `MmioRegion` dengan base dan size. Tolak akses di luar range.

## Struct register

Pakai struct bitfield + `static_assert(sizeof)`. Jangan shift magis tersebar.
Contoh: `ESR_EL1 { EC[31:26], IL[25], ISS[24:0] }`.
