# GEMA System Layer (GSL)

Status: keputusan inti sudah dibuat. GSL-1 selesai dan GSL-2 berjalan
untuk binary dynamic serta `ld-gem`. GSLFS dan init user belum ada.

GSL adalah arsitektur userland dan packaging untuk opengema-kernel. Tujuannya
memisahkan akses hardware dari aplikasi, membuat dependency eksplisit, dan
membuat kegagalan satu aplikasi tidak merusak kernel atau aplikasi lain.

## 1. Tujuan

1. Aplikasi tidak pernah memanggil `svc` atau menyentuh MMIO, DMA, dan IRQ.
2. Aplikasi hanya memakai API publik `libgemapi`.
3. `libkgema` menjadi lapisan ABI kernel yang privat.
4. Binary aplikasi dan library berasal dari filesystem, bukan ditanam di
   kernel.
5. Setiap layer punya validasi dan batas kegagalan sendiri.
6. Layout filesystem stabil dan cukup mirip Linux atau Windows agar mudah
   dipahami.

## 2. Lima layer

```
+--------------------------------------------------------------+
| Applications  gemabox, editor, shell, aplikasi lain          |
+--------------------------------------------------------------+
| API           libgemapi, header publik, capability, versi    |
+--------------------------------------------------------------+
| Libraries     libkgema, runtime C++, loader, driver user     |
+--------------------------------------------------------------+
| Kernel        syscall, proses, memori, VFS, driver hardware  |
+--------------------------------------------------------------+
| Hardware      CPU, RAM, interrupt, PCI, block, display, net  |
+--------------------------------------------------------------+
```

Arah dependensi hanya ke bawah:

```
gemabox -> libgemapi -> libkgema -> syscall -> kernel -> hardware
```

Tidak boleh ada panah ke atas. Aplikasi tidak boleh mengimpor header kernel
atau `libkgema`.

## 3. Status implementasi saat ini

Kode sekarang berada di awal GSL:

- `packages/libgemapi` sudah ada dan menjadi satu-satunya API aplikasi.
- `packages/libkgema` adalah ABI privat di bawah `libgemapi`; hanya
  `libkgema` yang berisi instruksi `svc`.
- `gemabox` hanya memakai header `gemapi/` dan menautkan `libgemapi.a`.
- `libkgema.so.1` dan `libgemapi.so.1` dimuat oleh `ld-gem` dari
  `/system/libraries` pada volume system GSLFS.
- Kernel memuat `/applications/gemabox/gemabox` sebagai ELF64 dynamic,
  membaca `PT_INTERP`, dan memetakan `/system/boot/ld-gem`.
- Root `/` adalah volume GSLFS read-only. `/dev`, `/proc`, `/tmp`, dan
  `/volumes/boot` sudah di-mount.
- `scripts/check-gsl.sh` menolak aplikasi yang menyentuh lapisan bawah.
- `/etc`, `/users`, dan `/var` writable serta init user belum ada.
- Binary dan library tidak lagi disalin kernel ke ramfs.

Dokumen GSL ini adalah target arsitektur, bukan deskripsi kode yang sudah
selesai.

## 4. Keputusan brainstorming

| Topik | Keputusan |
|---|---|
| API aplikasi | Hanya `libgemapi` |
| ABI kernel user | Hanya `libkgema`, disembunyikan dari aplikasi |
| Binary | ELF64 dynamic, dimuat dari filesystem |
| Library | `.so` di `/system/libraries`, bukan ditanam di kernel |
| Mount | Satu volume system, bukan satu mount per library |
| System volume | Filesystem GSL khusus, read-only |
| Layout | Unix-like lowercase, satu root, system read-only |
| Home | `/users/<user>` writable |
| Aplikasi | `/applications/<package>`, command di `/commands` |
| Init | Kernel menjalankan `/system/boot/init` sebagai proses pertama |
| Shell | Proses user, bukan bagian kernel |
| Isolasi | Satu proses per aplikasi, address space sendiri |
| Kegagalan | Error dikembalikan ke pemanggil, kernel tidak ikut gagal |
| Library strategy | Dynamic langsung, dengan bootstrap linker statis |
| Bentuk command | Hybrid: applet inti di gemabox, aplikasi besar terpisah |
| Manifest | `key=value` |
| Capability | Hybrid path dan handle |

## 5. Dokumen

| Berkas | Isi |
|---|---|
| `01-layers.md` | Definisi layer dan aturan dependensi |
| `02-libraries-and-api.md` | Pembagian libkgema dan libgemapi |
| `03-filesystem-layout.md` | Layout filesystem dan packaging |
| `04-boot-and-exec.md` | Boot, init, mount, dan alur exec |
| `05-security-and-failure.md` | Isolasi, capability, dan recovery |
| `06-roadmap.md` | Urutan pekerjaan GSL |
| `07-open-questions.md` | Opsi yang masih dibahas |
| `08-gslfs-format.md` | Format filesystem GSL read-only |

## 6. Prinsip singkat

1. Hardware hanya boleh disentuh kernel.
2. Aplikasi hanya boleh bicara ke API.
3. API harus punya kontrak yang stabil dan bisa gagal dengan aman.
4. Library tidak boleh memberi jalan pintas ke kernel.
5. Semua yang bukan kernel harus berasal dari filesystem yang sudah
   dipasang.
6. Setiap kegagalan harus berhenti di proses yang gagal jika memungkinkan.
