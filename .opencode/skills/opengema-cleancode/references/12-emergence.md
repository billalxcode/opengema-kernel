# Bab 12 - Emergence

Sumber: Clean Code, bab 12.

## Inti

Desain bersih muncul dari penerapan aturan sederhana secara disiplin, bukan
dari arsitektur besar di awal. Empat aturan desain sederhana:

1. Menjalankan semua test.
2. Tidak ada duplikasi.
3. Mengekspresikan maksud programmer.
4. Meminimalkan jumlah class dan method.

Urutannya penting: test dulu, lalu duplikasi, lalu ekspresi, lalu
minimalisasi.

## Aturan

- Jangan menambah abstraksi spekulatif.
- Jangan menambah pattern tanpa masalah nyata.
- Hapus duplikasi saat ditemukan.
- Buat kode mengungkap maksud lewat nama, struktur, dan test.
- Jangan mengurangi jumlah class dengan mengorbankan SRP.
- Refactor kecil setiap kali test hijau.

## Penerapan di OpenGEMA

- Mulai dari implementasi paling kecil yang lulus test.
- Jangan membuat framework driver sebelum ada driver kedua.
- Jangan membuat abstraction layer tanpa dua implementasi nyata.
- Duplikasi path, parsing, dan validasi harus diekstrak.
- Ekspresikan maksud dengan nama domain: `MapWritable`, `FileRead`.
- Jangan menambah class hanya untuk membungkus satu fungsi.
- Jika `SetupWritableFs` atau `gslfs::Mount` terlalu besar, pecah setelah
  test tersedia.

## Pertanyaan review

- Apakah semua test hijau?
- Apakah ada duplikasi yang bisa dihapus?
- Apakah kode mengungkap maksud tanpa penjelasan tambahan?
- Apakah ada class atau method yang tidak perlu?
- Apakah abstraksi ini punya dua pemakai nyata?

## Referensi

- Buku: Robert C. Martin, Clean Code, Prentice Hall, 2008,
  ISBN 978-0-13-235088-4, bab 12.
- Daftar isi resmi:
  https://catdir.loc.gov/catdir/toc/ecip0820/2008024750.html
- O'Reilly bab 12:
  https://www.oreilly.com/library/view/clean-code-a/9780136083238/chapter12.xhtml
- Catatan bab 12:
  https://github.com/JuanCrg90/Clean-Code-Notes#chapter12
- Ringkasan umum:
  https://gist.github.com/wojteklu/73c6914cc446146b8b533c0988cf8d29
