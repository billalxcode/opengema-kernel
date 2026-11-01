# Bab 17 - Smells and Heuristics

Sumber: Clean Code, bab 17.

## Inti

Bab ini adalah katalog smell dan heuristic yang dikumpulkan dari studi
kasus. Smell adalah gejala, bukan diagnosis pasti. Gunakan sebagai pemicu
review, bukan alasan refactor otomatis.

Kategori utama:

- Comments: komentar tidak tepat, redundan, usang, kode mati.
- Environment: build dan test harus satu langkah, mudah dijalankan.
- Functions: terlalu banyak argumen, flag argumen, output argumen,
  duplikasi, dead code, salah tingkat abstraksi.
- General: duplikasi, kode mati, salah tempat, coupling, god object,
  magic number, conditional kompleks, artificial coupling.
- Names: nama tidak deskriptif, menyesatkan, encoding, inconsistensi.
- Tests: test tidak cukup, tidak jelas, lambat, rapuh, tidak independen.
- Java-specific: konvensi bahasa yang tidak berlaku langsung di C++.

## Aturan

- Gunakan smell sebagai pertanyaan, bukan tuduhan.
- Perbaiki smell yang berdampak pada perubahan saat ini.
- Jangan refactor semua smell sekaligus.
- Prioritaskan duplikasi, nama menyesatkan, error hilang, dan test rapuh.
- Catat smell besar yang ditunda.

## Penerapan di OpenGEMA

Smell yang paling relevan:

- Duplikasi path dan parsing.
- Fungsi setup yang panjang.
- Komentar yang mengulang kode.
- Error yang di-`(void)` tanpa alasan.
- Magic address dan magic number.
- Lock yang memegang terlalu lama.
- Test yang hanya menguji jalur sukses.
- Global state baru.
- Abstraksi spekulatif.
- File yang mencampur driver, policy, dan formatting.

Heuristic review cepat:

1. Apakah ada duplikasi?
2. Apakah nama menyesatkan?
3. Apakah fungsi terlalu besar?
4. Apakah error hilang?
5. Apakah test cukup?
6. Apakah shared state aman?
7. Apakah ada kode mati?
8. Apakah ada magic number?
9. Apakah komentar masih benar?
10. Apakah perubahan ini bisa lebih kecil?

## Referensi

- Buku: Robert C. Martin, Clean Code, Prentice Hall, 2008,
  ISBN 978-0-13-235088-4, bab 17.
- Daftar isi resmi:
  https://catdir.loc.gov/catdir/toc/ecip0820/2008024750.html
- O'Reilly bab 17:
  https://www.oreilly.com/library/view/clean-code-a/9780136083238/chapter17.xhtml
- Catatan bab 17:
  https://github.com/JuanCrg90/Clean-Code-Notes#chapter17
- Ringkasan bab 17:
  https://vivekkhatri.com/chapter-17-smells-heuristics-clean-code-robert-c-martin/
- Ringkasan bab 17 lain:
  http://nicolecarpenter.github.io/2016/03/29/clean-code-chapter-17-smells-and-heuristics.html
- Kutipan bab 17:
  https://mosquitolwz.medium.com/clean-code-quotes-17-smells-and-heuristics-513bca047017
