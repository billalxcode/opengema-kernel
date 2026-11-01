# GEMA System Layer (GSL)

Status: draft untuk brainstorming. Belum ada keputusan final.

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

Kode sekarang masih berada pada tahap sebelum GSL:

- `packages/libkgema` berisi syscall, startup, io, file, memory, dan proses.
- `packages/gemabox` memakai `libkgema` langsung.
- Binary gemabox masih dimasukkan ke ramfs oleh kernel.
- Belum ada `libgemapi`, init user, atau system volume.

Dokumen GSL ini adalah target arsitektur, bukan deskripsi kode yang sudah
selesai.

## 4. Keputusan awal yang diusulkan

| Topik | Usulan |
|---|---|
| API aplikasi | Hanya `libgemapi` |
| ABI kernel user | Hanya `libkgema`, disembunyikan dari aplikasi |
| Binary | ELF64, dimuat dari filesystem |
| Library | File di `/System/Libraries`, bukan ditanam di kernel |
| Mount | Volume dipasang ke path, bukan satu mount per library |
| Init | Kernel menjalankan `/System/Boot/init` sebagai proses pertama |
| Shell | Proses user, bukan bagian kernel |
| Isolasi | Satu proses per aplikasi, address space sendiri |
| Kegagalan | Error dikembalikan ke pemanggil, kernel tidak ikut gagal |
| System volume | Read-only, data user dipisah |
| Tahap awal | Library statis dulu, dynamic loader menyusul |

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

## 6. Prinsip singkat

1. Hardware hanya boleh disentuh kernel.
2. Aplikasi hanya boleh bicara ke API.
3. API harus punya kontrak yang stabil dan bisa gagal dengan aman.
4. Library tidak boleh memberi jalan pintas ke kernel.
5. Semua yang bukan kernel harus berasal dari filesystem yang sudah
   dipasang.
6. Setiap kegagalan harus berhenti di proses yang gagal jika memungkinkan.
