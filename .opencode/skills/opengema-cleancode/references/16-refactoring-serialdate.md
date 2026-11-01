# Bab 16 - Refactoring SerialDate

Sumber: Clean Code, bab 16.

## Inti

SerialDate adalah studi kasus refactor kode tanggal yang berantakan.
Penulis menunjukkan bagaimana test, penamaan, pemisahan tanggung jawab, dan
penghapusan kode mati mengubah kode yang sulit dipercaya menjadi lebih
jelas.

Pelajaran utama:

- Tulis test karakterisasi sebelum mengubah kode lama.
- Perbaiki nama yang menyesatkan.
- Pisahkan tanggung jawab tanggal, format, dan kalender.
- Hapus komentar dan kode yang tidak relevan.
- Ubah magic number menjadi konstanta bernama.
- Jangan mengubah perilaku tanpa test.

## Aturan

- Pahami perilaku lama sebelum refactor.
- Tambahkan test karakterisasi.
- Refactor dalam langkah kecil.
- Jangan mencampur perbaikan bug dan refactor.
- Hapus kode mati setelah test membuktikan tidak dipakai.
- Gunakan konstanta bernama untuk tanggal dan waktu.

## Penerapan di OpenGEMA

- RTC PL031 dan konversi waktu adalah kandidat penerapan.
- Jangan memperkenalkan timezone atau kalender kompleks sebelum ada
  kebutuhan.
- Pisahkan pembacaan RTC, konversi, dan formatting.
- Test nilai epoch, leap year, dan detik nol.
- Jangan menaruh logika tanggal di driver hardware.
- Gunakan tipe dan konstanta yang jelas, bukan angka ajaib.

## Pertanyaan review

- Apakah perilaku lama sudah dites sebelum refactor?
- Apakah nama tanggal/waktu jelas?
- Apakah ada magic number?
- Apakah formatting dan kalender dipisah?
- Apakah kode mati sudah dihapus?

## Referensi

- Buku: Robert C. Martin, Clean Code, Prentice Hall, 2008,
  ISBN 978-0-13-235088-4, bab 16.
- Daftar isi resmi:
  https://catdir.loc.gov/catdir/toc/ecip0820/2008024750.html
- O'Reilly bab 16:
  https://www.oreilly.com/library/view/clean-code-a/9780136083238/chapter16.xhtml
- Catatan bab 16:
  https://github.com/JuanCrg90/Clean-Code-Notes
- Ringkasan umum:
  https://gist.github.com/wojteklu/73c6914cc446146b8b533c0988cf8d29
