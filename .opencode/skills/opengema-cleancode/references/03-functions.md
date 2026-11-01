# Bab 3 - Functions

Sumber: Clean Code, bab 3.

## Inti

Fungsi harus kecil, melakukan satu hal, dan berada pada satu level
abstraksi. Fungsi yang panjang biasanya mencampur beberapa tugas dan sulit
dites.

Prinsip utama:

- Small. Lebih kecil lebih baik.
- Do one thing.
- One level of abstraction per function.
- Baca kode dari atas ke bawah seperti cerita.
- Switch statement sebaiknya disembunyikan di factory atau polimorfisme.
- Nama fungsi deskriptif dan konsisten.
- Argumen sedikit: nol sampai tiga.
- Hindari flag argument yang mengubah perilaku.
- Hindari output argument jika bisa return nilai.
- Command query separation: fungsi mengubah state atau mengembalikan data,
  jangan keduanya tanpa alasan.
- Error handling adalah satu tugas tersendiri.
- DRY: jangan duplikasi logika.
- Structured programming: satu entry, satu exit, hindari `goto`.

## Aturan

- Guard clause untuk validasi di awal.
- Maksimal satu atau dua level nesting untuk path utama.
- Ekstrak blok yang butuh komentar menjadi fungsi bernama.
- Jangan mengembalikan `-1` mentah tanpa kontrak.
- Jangan mencampur I/O, validasi, dan mutasi state dalam satu fungsi.
- Jika fungsi butuh lebih dari tiga argumen, pertimbangkan struct parameter.
- Jika fungsi punya banyak `bool` flag, pecah fungsi.

## Penerapan di OpenGEMA

- Kernel memakai `Status` dan `Result<T>` untuk operasi fallible.
- Tanpa exception. Clean Code menyarankan exception, tetapi proyek ini
  freestanding; gunakan `Status`/`Result` dan guard clause.
- Output parameter masih dipakai di syscall dan ABI (`uint64_t& result`).
  Itu batas ABI; beri kontrak dan jangan menyebar ke API publik.
- `libgemapi` memakai `Result<T>` dan `Status`, bukan kode error mentah.
- Fungsi driver harus memisahkan validasi, akses register, dan DMA.
- Fungsi syscall harus memisahkan copy user, validasi, dan aksi.
- `SetupWritableFs` saat ini panjang; kandidat ekstraksi menjadi
  `CreateWritableTree`, `MountWritable`, dan `SeedEtc`.

## Pertanyaan review

- Apakah fungsi melakukan lebih dari satu hal?
- Apakah ada level abstraksi yang tercampur?
- Apakah argumen bisa dikurangi?
- Apakah ada flag argument yang membingungkan?
- Apakah error dan cleanup tercampur dengan logika utama?
- Apakah ada duplikasi yang bisa diekstrak?

## Referensi

- Buku: Robert C. Martin, Clean Code, Prentice Hall, 2008,
  ISBN 978-0-13-235088-4, bab 3.
- Daftar isi resmi:
  https://catdir.loc.gov/catdir/toc/ecip0820/2008024750.html
- O'Reilly bab 3:
  https://www.oreilly.com/library/view/clean-code-a/9780136083238/chapter03.xhtml
- Catatan bab 3:
  https://github.com/JuanCrg90/Clean-Code-Notes#chapter3
- Ringkasan bab 3:
  https://vuyisile.com/clean-code-chapter-3-functions-summary/
