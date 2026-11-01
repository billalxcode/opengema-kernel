# Bab 7 - Error Handling

Sumber: Clean Code, bab 7.

## Inti

Error handling adalah bagian penting dari desain, bukan afterthought.
Kode harus tetap jelas saat menangani kegagalan. Clean Code menyarankan
exception, tetapi prinsip intinya adalah memisahkan alur normal dari alur
error dan tidak menyembunyikan kegagalan.

Prinsip utama:

- Jangan campur error handling dengan logika utama.
- Tulis jalur error lebih dulu jika memungkinkan.
- Beri konteks pada error.
- Definisikan kelas error berdasarkan kebutuhan pemanggil.
- Jangan return null; jangan pass null.
- Jangan mengabaikan error.
- Sediakan cleanup yang pasti.
- Bedakan error yang bisa dipulihkan dan invariant violation.

## Aturan

- Operasi fallible harus punya hasil yang jelas.
- Jangan memakai `-1` mentah tanpa dokumentasi.
- Jangan menelan error di driver atau syscall.
- Validasi input user sebelum memakai pointer.
- Cleanup harus berjalan di semua jalur.
- Panic hanya untuk invariant kernel yang tidak bisa dipulihkan.
- Error message harus menyebut operasi dan objek yang gagal.

## Penerapan di OpenGEMA

- Kernel memakai `Status` dan `Result<T>`; tidak ada exception.
- User API memakai `gemapi::Status` dan `gemapi::Result<T>`.
- `[[nodiscard]]` dipakai untuk operasi fallible.
- `IsErr` dan `IsOk` adalah helper standar.
- Syscall memvalidasi pointer, panjang, dan descriptor.
- `(void)FsClose(...)` boleh untuk cleanup yang tidak bisa gagal, tetapi
  jangan `(void)` untuk error yang penting.
- `Panic` hanya untuk kondisi mustahil atau korupsi fatal.
- Setiap driver harus punya test error: device tidak ada, DMA gagal,
  checksum salah, file hilang.

## Pertanyaan review

- Apakah setiap error path mengembalikan status yang berguna?
- Apakah ada error yang diabaikan?
- Apakah cleanup berjalan saat gagal?
- Apakah input user divalidasi?
- Apakah panic dipakai hanya untuk invariant violation?
- Apakah error message cukup konteks?

## Referensi

- Buku: Robert C. Martin, Clean Code, Prentice Hall, 2008,
  ISBN 978-0-13-235088-4, bab 7.
- Daftar isi resmi:
  https://catdir.loc.gov/catdir/toc/ecip0820/2008024750.html
- O'Reilly bab 7:
  https://www.oreilly.com/library/view/clean-code-a/9780136083238/chapter07.xhtml
- Catatan bab 7:
  https://github.com/JuanCrg90/Clean-Code-Notes#chapter7
- Ringkasan umum:
  https://gist.github.com/wojteklu/73c6914cc446146b8b533c0988cf8d29
