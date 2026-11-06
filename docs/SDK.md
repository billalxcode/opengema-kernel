# opengema SDK

SDK ini berisi header, library, linker script, dan CMake package untuk
membangun aplikasi userland opengema. Aplikasi hanya memakai `libgemapi`.
`libkgema` disediakan untuk komponen sistem yang memang butuh ABI kernel.

## Isi paket

- `include/gemapi`: API aplikasi.
- `include/gsl`: layout path tetap.
- `include/cxx`: header C++ minimal untuk lingkungan freestanding.
- `include/kgema`: ABI kernel, hanya untuk komponen sistem.
- `lib/libgemapi.a`: archive aplikasi, sudah membawa objek kgema.
- `lib/libkgema.a`: archive ABI mentah.
- `lib/libgemapi.so.1` dan `lib/libkgema.so.1`: library dynamic.
- `lib/opengema/link.ld`: linker script PIE untuk aplikasi dynamic.
- `lib/opengema/link-static.ld`: linker script fixed-base untuk archive
  statis dan recovery build.
- `lib/opengema/opengema-sdk-toolchain.cmake`: toolchain AArch64.
- `lib/cmake/gemapi`: CMake package config.

## Build kernel dan install SDK

Dari root repository:

```sh
./scripts/build.sh
cmake --install build --prefix "$PWD/build/sdk"
```

Perintah install membutuhkan hasil build karena archive dan shared library
dibuat oleh build tersebut.

## Build contoh aplikasi

```sh
cmake -S examples/hello -B build/sdk-hello \
  --toolchain "$PWD/build/sdk/lib/opengema/opengema-sdk-toolchain.cmake" \
  -DCMAKE_PREFIX_PATH="$PWD/build/sdk" \
  -G Ninja
cmake --build build/sdk-hello
```

Hasilnya adalah `build/sdk-hello/hello`, ELF64 AArch64 freestanding.

## CMake target

```cmake
find_package(gemapi CONFIG REQUIRED)

add_executable(app main.cpp)
target_link_libraries(app PRIVATE gemapi::gemapi)
```

Target `gemapi::gemapi` membawa:

- opsi compile freestanding
- include path SDK
- linker script SDK
- entry point `_start`
- opsi link tanpa libc

Target `gemapi::kgema` tersedia untuk komponen sistem yang perlu memanggil
ABI kernel secara langsung. Aplikasi biasa tidak perlu target itu.

## Catatan

- Toolchain memakai `clang` dengan target `aarch64-linux-gnu` dan
  `aarch64-linux-gnu-ld`.
- Tidak ada libc, exception, RTTI, atau thread-local storage.
- Konstruktor global C++ didukung lewat `.init_array`.
- Untuk menjalankan hasil build di image, salin binary ke
  `/applications/<nama>/<nama>` di volume system.
