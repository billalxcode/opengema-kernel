# libgemapi

API publik GEMA System Layer. Aplikasi seperti `gemabox` hanya memakai
header `gemapi/...` dan hanya menautkan library ini.

`libgemapi` memakai `libkgema` secara privat. Aplikasi tidak boleh melihat
header `kgema/`, nomor syscall, atau instruksi `svc`.

## Isi

| Header | Fungsi |
|---|---|
| `gemapi/types.h` | Tipe dasar `usize`, `u64`, `i64`, dan `kPageSize` |
| `gemapi/error.h` | `Status` dan `Result<T>` untuk semua operasi fallible |
| `gemapi/io.h` | Console, `print`, dan `read_line` |
| `gemapi/fs.h` | `File`, `open`, `read`, `write`, `seek`, `stat`, `chdir`, `list`, `chmod`, `mount`, `umount` |
| `gemapi/memory.h` | `map` dan `unmap` |
| `gemapi/paths.h` | Path tetap GSL: `/system`, `/applications`, `/tmp`, dan lainnya |
| `gemapi/process.h` | `id`, `spawn`, `wait`, `exit`, `user_id`, `group_id`, `set_user_id`, dan `set_group_id` |
| `gemapi/system.h` | Versi API |
| `gemapi/gemapi.h` | Umbrella header |

## Pemakaian

```cpp
#include "gemapi/fs.h"
#include "gemapi/io.h"

int main(int argc, char** argv, char** envp) {
  (void)argc;
  (void)argv;
  (void)envp;

  gemapi::io::write("hello from el0\n");
  auto file = gemapi::fs::File::open("/ram/hello");
  if (!file) {
    gemapi::io::write("open failed: ");
    gemapi::io::print(gemapi::to_string(file.status()));
    gemapi::io::write("\n");
    return 1;
  }
  char text[32];
  auto count = file.value().read(text, sizeof(text));
  if (count) {
    gemapi::io::write(text, count.value());
  }
  return 0;
}
```

`File` memiliki satu deskriptor dan menutupnya otomatis. `File` tidak bisa
disalin agar descriptor tidak ditutup dua kali.

## Desain error

Semua operasi fallible mengembalikan `Status` atau `Result<T>`. API tidak
mengembalikan `-1` mentah. Saat ini kernel masih mengembalikan `-1` untuk
semua kegagalan, jadi `libgemapi` memetakan error menurut operasi:

| Operasi | Status awal |
|---|---|
| `open`, `stat`, `chdir`, `spawn` | `not_found` |
| `read`, `write`, `seek`, `wait` | `io` |
| `map` | `no_memory` |
| argumen null atau ukuran tidak valid | `invalid_argument` |

Kode error yang lebih tepat menyusul saat ABI kernel punya nilai error.

## Build

CMake proyek utama menghasilkan:

```
build/libgemapi.a
build/libgemapi.so.1
build/libgemapi.so -> libgemapi.so.1
```

`libgemapi.a` adalah satu kesatuan link: berisi objek API, `_start`, dan
objek `libkgema` privat. Baris link aplikasi hanya menyebut `libgemapi.a`.

`libgemapi.so.1` memiliki `DT_NEEDED` ke `libkgema.so.1` dan dimuat oleh
`ld-gem`. Executable dynamic menautkan `_start` dari objek runtime terpisah
dan `libgemapi.so`; executable statis memakai arsip `libgemapi.a`.

## Aturan

- Aplikasi hanya menyertakan `gemapi/...`.
- Aplikasi hanya menautkan `libgemapi`.
- Header publik tidak boleh menyertakan `kgema/` atau `og/`.
- `libgemapi` tidak boleh menyentuh MMIO, DMA, atau IRQ.
- `libgemapi` tidak boleh menyimpan kebijakan keamanan sendiri; capability
  diperiksa di API dan kernel pada GSL-6.
