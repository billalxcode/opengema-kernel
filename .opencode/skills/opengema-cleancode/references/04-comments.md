# Bab 4 - Comments

Sumber: Clean Code, bab 4.

## Inti

Komentar tidak menutupi kode yang buruk. Jika kode butuh komentar untuk
menjelaskan apa yang dilakukannya, tulis ulang kode agar maksudnya jelas.
Komentar yang baik menjelaskan why, bukan what.

Komentar yang berguna:

- Legal dan lisensi.
- Informasi yang tidak bisa diungkapkan kode, misalnya batasan hardware.
- Penjelasan intent atau alasan keputusan.
- Klarifikasi terhadap API atau format eksternal.
- Peringatan konsekuensi.
- `TODO` dengan pemilik dan alasan.
- Amplifikasi: menonjolkan detail penting yang mudah terlewat.
- Dokumentasi API publik.

Komentar yang buruk:

- Mumbling dan komentar tidak jelas.
- Redundant, mengulang kode.
- Misleading atau usang.
- Komentar wajib yang tidak menambah nilai.
- Journal dan catatan perubahan.
- Noise, position marker, closing brace.
- Komentar kode mati.
- Informasi nonlocal.
- Komentar fungsi yang tidak konsisten.
- Javadoc untuk kode nonpublik yang tidak perlu.

## Aturan

- Usahakan maksud terlihat dari nama dan tipe.
- Komentar harus tetap benar saat kode berubah.
- Jangan mengomentari kode mati; hapus.
- `TODO` wajib punya pemilik: `// TODO(owner): alasan`.
- Jangan menaruh sejarah perubahan di komentar; gunakan git.
- Komentar bahasa Indonesia atau Inggris harus konsisten dalam satu file.

## Penerapan di OpenGEMA

Komentar yang diperlukan:

- Quirk hardware dan urutan register.
- Lock ordering dan aturan concurrency.
- Kepemilikan memori dan lifetime.
- Kontrak ABI dan syscall.
- Alasan sebuah magic value berasal dari spesifikasi.
- Batasan freestanding, tanpa exception, dan tanpa libc.

Komentar yang harus dihindari:

- `// increment i` sebelum `++i`.
- `// loop` sebelum `for`.
- Kode lama yang dikomentari.
- Komentar yang bertentangan dengan kode.
- `// temporary` tanpa rencana.

## Pertanyaan review

- Apakah komentar menjelaskan why, bukan what?
- Apakah komentar masih benar?
- Apakah ada kode mati yang dikomentari?
- Apakah `TODO` punya pemilik dan alasan?
- Apakah komentar bisa diganti dengan nama yang lebih baik?

## Referensi

- Buku: Robert C. Martin, Clean Code, Prentice Hall, 2008,
  ISBN 978-0-13-235088-4, bab 4.
- Daftar isi resmi:
  https://catdir.loc.gov/catdir/toc/ecip0820/2008024750.html
- O'Reilly bab 4:
  https://www.oreilly.com/library/view/clean-code-a/9780136083238/chapter04.xhtml
- Catatan bab 4:
  https://github.com/JuanCrg90/Clean-Code-Notes#chapter4
- Ringkasan umum:
  https://gist.github.com/wojteklu/73c6914cc446146b8b533c0988cf8d29
