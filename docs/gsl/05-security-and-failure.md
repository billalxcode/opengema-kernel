# 05 - Keamanan dan Batas Kegagalan

Dokumen ini membahas bagaimana setiap layer GSL mencegah kegagalan kecil
menjadi kerusakan sistem.

## 1. Model ancaman

Yang perlu diasumsikan:

- Aplikasi user bisa memiliki bug pointer, buffer, atau ukuran.
- Aplikasi bisa sengaja mencoba membuka file atau device yang tidak boleh.
- File di volume data bisa rusak.
- Library pihak ketiga bisa salah mengelola handle.
- Driver kernel bisa memiliki bug.
- Sistem bisa kehabisan memori, thread, atau file descriptor.
- Proses bisa crash saat memegang resource.

## 2. Batas keamanan per layer

| Layer | Batas | Mekanisme |
|---|---|---|
| Hardware | fisik | firmware, IOMMU nanti |
| Kernel | EL1 | MMU, W^X, validasi user copy, capability |
| Library | user space | handle, bounds, error, read-only code |
| API | kontrak | versi, tipe, kebijakan resource |
| Application | proses | address space sendiri, exit terisolasi |

## 3. Capability

Capability adalah hak akses eksplisit yang diberikan ke proses.

Contoh:

- `console.write`
- `console.read`
- `filesystem.read`
- `filesystem.write:/Home`
- `process.spawn`
- `device.block0.read`
- `network.udp`

Aturan:

- Proses tidak mendapat semua hak secara default.
- Capability tidak boleh berupa path string mentah tanpa pemeriksaan.
- Handle bisa dicabut atau ditutup.
- Capability yang tidak dimiliki menghasilkan error, bukan panic.

Tahap awal bisa memakai daftar capability sederhana di manifest. Model
handle penuh menyusul.

## 4. Validasi syscall

Kernel wajib memeriksa:

- Pointer user berada di region yang terdaftar.
- Panjang tidak overflow.
- Buffer tidak melewati batas region.
- File descriptor valid dan masih terbuka.
- Proses memiliki izin untuk resource.
- Ukuran alokasi berada di batas yang wajar.

Kesalahan harus dikembalikan sebagai status. Kernel tidak boleh memakai
pointer user secara langsung tanpa copy atau translasi.

## 5. Isolasi aplikasi

- Setiap aplikasi berjalan di proses sendiri.
- Setiap proses punya address space dan ASID sendiri.
- Stack kernel dan stack user dipisahkan.
- Aplikasi tidak bisa melihat memori proses lain.
- Aplikasi yang crash tidak boleh memicu panic kernel.
- Shell atau service manager dapat menjalankan ulang aplikasi.

## 6. Batas resource

Setiap proses perlu batas:

- Jumlah page.
- Jumlah thread.
- Jumlah file descriptor.
- Ukuran stack.
- Waktu CPU.
- Jumlah child process.
- Ukuran output console.

Jika batas tercapai, API mengembalikan `NoMemory`, `Busy`, atau
`PermissionDenied`. Sistem tidak boleh membunuh kernel.

## 7. Kegagalan library

- Library dimuat read-only.
- Handle divalidasi sebelum dipakai.
- Library tidak boleh memegang lock kernel.
- Error dari `libkgema` diterjemahkan oleh `libgemapi`.
- Library yang crash hanya mematikan proses pemanggil.

## 8. Kegagalan filesystem

- System dan Applications read-only.
- Data user dipisah dari system.
- File yang hilang atau rusak menghasilkan error, bukan panic.
- Update system memakai volume baru dan switch setelah validasi.
- Recovery shell dapat dijalankan jika init gagal.

## 9. Penegakan saat build

Usulan pemeriksaan:

1. Aplikasi tidak boleh menyertakan `kgema/`.
2. Aplikasi tidak boleh memiliki `svc` di source atau object.
3. Aplikasi hanya menautkan `libgemapi`.
4. `libgemapi` tidak boleh menyertakan header kernel internal.
5. `libkgema` tidak boleh menyertakan driver atau MMIO.
6. Simbol internal tidak diekspor ke aplikasi.

## 10. Penegakan saat runtime

- Capability diperiksa di API dan kernel.
- Handle palsu ditolak.
- Path dinormalisasi sebelum dipakai.
- Operasi tulis ke volume read-only ditolak.
- Proses tidak boleh memetakan halaman executable yang writable.
- Dynamic linker hanya memuat library dari path tepercaya.

## 11. Test keamanan

Minimal:

- Pointer user null, tidak termap, dan melewati batas.
- Ukuran buffer overflow.
- File descriptor salah dan sudah ditutup.
- Path terlalu panjang dan path traversal.
- Spawn binary yang bukan ELF.
- Library yang hilang atau versi salah.
- Proses yang crash saat menulis console.
- Kehabisan memori dan thread.

## 12. Yang masih dibahas

- Apakah capability berbasis handle atau path.
- Apakah aplikasi boleh membaca seluruh filesystem.
- Apakah network butuh capability terpisah.
- Apakah user driver akan diizinkan.
- Kapan IOMMU masuk.
