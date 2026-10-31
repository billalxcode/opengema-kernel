# Toolchain UEFI: clang + lld-link + header sendiri

Keputusan: link PE/COFF langsung pakai `lld-link`, tanpa objcopy ELF ke EFI.

Alasan:

- objcopy ELF ke `efi-app-aarch64` hasilkan image tanpa `.reloc`
  dan section berantakan. EDK2 tolak dengan `Unsupported`.
- `aarch64-linux-gnu-ld` di Debian tidak dukung emulasi `aarch64pe`.
- `lld-link` hasilkan PE valid: header benar, `.reloc` otomatis,
  subsystem EFI application, entry `efi_main`.

Konsekuensi: target `aarch64-unknown-windows` tidak punya header
C++ di container. Kernel vendor header freestanding sendiri di
`libs/cxx/`: `cstddef`, `cstdint`, `type_traits`, `concepts`, `span`.
Hanya yang kernel pakai. Tambah sesuai butuh, jangan impor libstdc++.

Flags compile: `--target=aarch64-unknown-windows -ffreestanding
-nostdinc -fshort-wchar -mno-red-zone -fno-exceptions -fno-rtti`.

Flags link: `lld-link /machine:arm64 /subsystem:efi_application
/entry:efi_main /nodefaultlib`.

Lihat `cmake/toolchain-aarch64-uefi.cmake` dan `CMakeLists.txt`.
