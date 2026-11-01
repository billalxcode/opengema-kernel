# 01 - Layer GSL

Dokumen ini menjelaskan batas setiap layer, apa yang boleh dipanggil, dan
apa yang harus ditolak.

## 1. Hardware

Isi:

- CPU, MMU, cache, interrupt controller, timer.
- RAM dan perangkat PCI.
- Block device, display, UART, jaringan, input.
- Firmware UEFI dan tabel hardware.

Aturan:

- Hanya kernel yang boleh mengakses MMIO, DMA, IRQ, dan instruksi
  istimewa.
- Aplikasi dan library tidak boleh memiliki alamat fisik atau register
  hardware.
- Driver user hanya boleh ada jika nanti dilindungi IOMMU dan capability
  eksplisit. Untuk sekarang semua driver ada di kernel.

## 2. Kernel

Isi:

- Arch, MMU, PMM, VMM, heap, scheduler, interrupt, timer.
- Driver hardware.
- VFS, block layer, filesystem.
- Process, address space, syscall, user copy.
- Mount table dan device registry.

Yang boleh dilakukan kernel:

- Menjalankan instruksi EL1.
- Mengakses hardware.
- Memetakan halaman user.
- Membaca dan menulis filesystem.
- Memuat executable ELF.
- Memvalidasi pointer dan capability dari user.

Yang tidak boleh dilakukan kernel:

- Menjalankan logika aplikasi.
- Menyimpan binary aplikasi di dalam image kernel.
- Memberi pointer mentah ke MMIO melalui syscall.
- Menganggap input user selalu benar.

Kernel tidak perlu menjadi monolithic untuk semua kebijakan user. Init,
package manager, dan shell adalah proses user.

## 3. Libraries

Isi:

- `libkgema`: pembungkus syscall dan ABI user.
- Runtime C++ minimal: startup, `init_array`, helper string, allocator.
- Loader library dan dynamic linker.
- Library pendukung yang tidak punya kebijakan aplikasi.

Aturan:

- Hanya `libkgema` yang boleh berisi instruksi `svc`.
- Library tidak boleh mengakses MMIO, DMA, atau IRQ.
- Library tidak boleh menjadi pemilik kebijakan keamanan.
- Library yang rusak harus membuat proses pemanggil gagal, bukan kernel.

## 4. API

Isi:

- `libgemapi`: API publik untuk aplikasi.
- Header versi, tipe, handle, error, dan dokumentasi kontrak.
- Capability dan pemeriksaan argumen.
- Adapter untuk filesystem, proses, memori, waktu, dan device.

Aturan:

- Aplikasi hanya boleh mengimpor `<gemapi/...>`.
- API tidak boleh membocorkan tipe internal kernel.
- Semua fungsi fallible mengembalikan `Result` atau `Status`, bukan
  `-1` tanpa arti.
- Semua handle divalidasi. Handle palsu atau sudah ditutup harus ditolak.
- API harus stabil atau punya versi yang jelas.

## 5. Applications

Isi:

- `gemabox` sebagai multi-call binary.
- Aplikasi tambahan seperti editor, file manager, atau shell.
- Program uji.

Aturan:

- Satu aplikasi berjalan di satu proses dan address space.
- Aplikasi tidak boleh memanggil `svc` langsung.
- Aplikasi tidak boleh memakai alamat fisik, MMIO, atau register kernel.
- Aplikasi yang crash tidak boleh mematikan kernel, shell, atau aplikasi
  lain.
- Aplikasi hanya boleh mengakses resource yang diberikan API.

## 6. Tabel izin

| Dari | Boleh memanggil | Tidak boleh |
|---|---|---|
| Application | libgemapi | libkgema, syscall, hardware |
| API | libkgema, runtime | hardware, kernel internal |
| Library | syscall lewat ABI, runtime | hardware, kebijakan user |
| Kernel | hardware, semua layer bawah | logika aplikasi |
| Hardware | tidak ada | semua layer atas |

## 7. Penegakan aturan

Penegakan dilakukan di tiga tempat:

1. Build: header dan link hanya boleh sesuai arah layer.
2. Runtime: capability, validasi pointer, dan hak akses.
3. Test: negative test untuk akses langsung, handle salah, dan argumen
   rusak.

Contoh aturan build:

- Aplikasi tidak boleh menyertakan `kgema/*.h`.
- Aplikasi tidak boleh memiliki inline assembly `svc`.
- Aplikasi hanya boleh menautkan `libgemapi`, bukan `libkgema`.
- `libgemapi` boleh menautkan `libkgema` secara privat.
- `libkgema` tidak boleh menyertakan header kernel internal.

## 8. Batas yang belum final

- Apakah runtime C++ masuk library atau API.
- Apakah dynamic linker bagian library atau service.
- Apakah user driver diizinkan nanti.
- Apakah library boleh membuka file sendiri atau hanya lewat API.
