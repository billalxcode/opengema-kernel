# Freestanding C++20

Sumber: P2338R4, OSDev Bare Bones C++, Core Guidelines P.2.

## Flags wajib

```
-ffreestanding -nostdlib -fno-exceptions -fno-rtti
-fno-threadsafe-statics -fno-unwind-tables
-Wall -Wextra -Werror -Wconversion -Wpedantic
```

## Header boleh

`cstdint, cstddef, span, array, string_view, concepts, type_traits, utility, optional, variant, atomic, bit, limits`.

## Header larang

`iostream, vector, string dengan alokasi, map, format dengan alokasi, stdexcept, exception, typeinfo, filesystem, thread, future`.

## Aturan

- Jika butuh throw, ganti `Status` atau `Expected<T>`.
- Jika butuh `dynamic_cast`, ganti tag enum.
- Sediakan `memset, memcpy, memmove` sendiri.
- Sediakan `operator new` placement saja. `new` yang throw dilarang link.
- Semua kode kernel adalah `noexcept` di path panas.
