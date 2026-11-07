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
| `gemapi/memory.h` | `map`, `map_lazy`, `map_shared`, `map_file`, dan `unmap` |
| `gemapi/sync.h` | `wait` dan `wake` untuk futex |
| `gemapi/paths.h` | Path tetap GSL: `/system`, `/applications`, `/tmp`, dan lainnya |
| `gemapi/process.h` | `id`, `spawn`, `fork`, `exec`, `wait`, `exit`, `user_id`, `group_id`, `set_user_id`, `set_group_id`, `priority`, `set_priority`, `tls`, `set_tls`, dan `tls_register` |
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

Semua operasi fallible mengembalikan `Status` atau `Result<T>`. Kernel
mengembalikan errno negatif di `x0`; `libkgema` menyimpan nilai terakhir dan
`libgemapi` memetakannya lewat `from_errno`.

| Errno | Status |
|---|---|
| `-EACCES`, `-EPERM` | `denied` |
| `-ENOENT` | `not_found` |
| `-EIO` | `io` |
| `-ENOMEM` | `no_memory` |
| `-EBUSY` | `busy` |
| `-EEXIST` | `exists` |
| `-ENODEV` | `no_device` |
| `-EINVAL` | `invalid_argument` |
| `-ENOSYS` | `not_implemented` |
| `-EOPNOTSUPP` | `not_supported` |
| `-ETIMEDOUT` | `timeout` |

Argumen null atau ukuran tidak valid tetap ditolak di API sebelum syscall.

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
