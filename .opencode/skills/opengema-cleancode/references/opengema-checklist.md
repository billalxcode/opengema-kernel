# Checklist OpenGEMA Clean Code

Gunakan checklist ini saat menulis diff atau review. Tidak semua item
berlaku untuk setiap perubahan. Pilih yang relevan.

## Sebelum menulis

- [ ] Tujuan perubahan bisa dijelaskan dalam satu kalimat.
- [ ] Layer yang disentuh sudah benar (hardware, kernel, libraries, API,
      applications).
- [ ] Test yang akan membuktikan perubahan sudah direncanakan.
- [ ] Tidak ada perubahan lain yang menumpang di diff.

## Nama

- [ ] Nama mengungkap maksud, bukan tipe atau encoding.
- [ ] Nama bisa diucapkan dan dicari.
- [ ] Tidak ada singkatan yang hanya dipahami penulis.
- [ ] Tipe memakai noun; fungsi memakai verb.
- [ ] Satu konsep memakai satu kata di seluruh modul.
- [ ] Panjang nama sesuai ukuran scope.
- [ ] Konstanta memakai `kPascalCase`; bukan angka ajaib.

## Fungsi

- [ ] Fungsi pendek dan punya satu tugas.
- [ ] Satu level abstraksi per fungsi.
- [ ] Maksimal tiga argumen; hindari boolean flag.
- [ ] Tidak ada output argumen jika `Result` lebih jelas.
- [ ] Command dan query dipisahkan.
- [ ] Guard clause untuk validasi, bukan nesting dalam.
- [ ] Tidak ada duplikasi logika.
- [ ] Nama fungsi menjelaskan efek samping.

## Komentar

- [ ] Komentar menjelaskan why, bukan what.
- [ ] Kode sudah jelas tanpa komentar pengulang.
- [ ] Tidak ada kode mati yang dikomentari.
- [ ] Tidak ada komentar jurnal atau catatan pribadi.
- [ ] `TODO` punya pemilik dan alasan.
- [ ] Komentar tidak menyesatkan atau usang.
- [ ] Komentar publik menjelaskan kontrak, error, dan ownership.

## Error

- [ ] Operasi fallible mengembalikan `Status` atau `Result`.
- [ ] Error tidak diabaikan dengan `(void)` tanpa alasan.
- [ ] Error diberi konteks yang cukup.
- [ ] Cleanup berjalan di semua jalur.
- [ ] Tidak return null; tidak pass null.
- [ ] Validasi pointer, panjang, dan overflow.
- [ ] Test jalur gagal ada.

## Class dan modul

- [ ] Satu alasan untuk berubah.
- [ ] Kohesi tinggi, coupling rendah.
- [ ] Interface kecil dan sesuai kebutuhan pemakai.
- [ ] Dependency mengarah ke layer bawah.
- [ ] Tidak ada god object atau global state baru.
- [ ] Construction dipisah dari use.
- [ ] Header tidak membocorkan detail implementasi.

## Test

- [ ] Test cepat, independen, repeatable, self-validating, timely.
- [ ] Satu konsep per test.
- [ ] Nama test menjelaskan perilaku.
- [ ] Assertion jelas dan tidak berlebihan.
- [ ] Test tidak bergantung pada urutan.
- [ ] Test memakai data dan environment yang deterministik.
- [ ] Negative test dan boundary test ada.

## Concurrency

- [ ] Shared state punya lock atau alasan jelas.
- [ ] Lock tidak dipegang saat blocking atau alokasi besar.
- [ ] Urutan lock konsisten.
- [ ] Data per CPU dipisahkan jika memungkinkan.
- [ ] Test concurrency dijalankan di SMP.
- [ ] Tidak ada asumsi single core.

## System dan boundary

- [ ] Adapter mengisolasi UEFI, PCI, virtio, block device, dan ABI.
- [ ] Tidak ada tipe eksternal bocor ke layer atas.
- [ ] Composition root jelas (kernel init atau init user).
- [ ] Kegagalan satu komponen tidak menjatuhkan kernel.
- [ ] Batas resource (memori, FD, thread) jelas.

## Konteks OpenGEMA

- [ ] Aplikasi hanya include `gemapi/`.
- [ ] Hanya `libkgema` yang berisi `svc`.
- [ ] Layout filesystem memakai `gsl/layout.h`.
- [ ] Tidak ada alamat board atau magic number di luar header pemilik.
- [ ] Tidak ada duplikasi memcpy, strlen, atau allocator.
- [ ] `check-gsl.sh`, `check-arch.sh`, dan `test.sh` hijau.
- [ ] ASCII saja, tanpa em dash.
