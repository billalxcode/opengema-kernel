# libgsl

Pemetaan filesystem GSL yang tetap. Library ini adalah sumber tunggal untuk
path sistem, mount point, dan lokasi file penting. Header-only, tanpa kode
runtime.

## Isi

```
include/gsl/layout.h
```

Konstanta utama:

```
/system
/system/boot
/system/boot/init
/system/boot/ld-gem
/system/libraries
/applications
/applications/gemabox/gemabox
/commands
/etc
/users
/tmp
/var
/dev
/proc
/volumes
/volumes/boot
```

## Pemakai

| Pemakai | Cara |
|---|---|
| Kernel | `#include "gsl/layout.h"` |
| `libgemapi` | `gemapi/paths.h` memakai konstanta `gsl::layout` |
| `ld-gem` | Mencari library di `gsl::layout::kLibraries` |
| Aplikasi | `gemapi::paths::...` |

## Aturan

- Layout adalah bagian ABI dan bersifat mutlak.
- User, aplikasi, dan service tidak boleh mengubah mount point.
- `gsl.conf` tidak boleh mendefinisikan `mount.*`.
- Volume tambahan hanya di `/volumes/<name>`.
- Perubahan layout memerlukan system image dan versi ABI baru.

`scripts/check-gsl.sh` menolak layout yang kembali ke `libs/core` atau
include lama `layout.h`.
