# Appendix C - Cross References of Heuristics

Sumber: Clean Code, appendix C.

## Inti

Appendix ini adalah indeks silang heuristic dan smell. Setiap item
menunjuk ke bab atau bagian yang membahasnya. Gunakan sebagai indeks cepat
saat review, bukan daftar lengkap yang harus dihafal.

Kategori yang bisa dicari:

- Comments
- Environment
- Functions
- General
- Names
- Tests
- Concurrency
- Java-specific

## Aturan

- Mulai dari smell yang terlihat, lalu cari bab yang relevan.
- Jangan memakai indeks sebagai alasan refactor otomatis.
- Prioritaskan smell yang berdampak pada perubahan saat ini.
- Catat smell besar yang ditunda.
- Perbaiki satu smell per commit jika memungkinkan.

## Penerapan di OpenGEMA

Gunakan appendix ini bersama `references/opengema-checklist.md`:

1. Identifikasi smell.
2. Cari bab yang membahas.
3. Baca reference bab.
4. Terapkan perbaikan kecil.
5. Jalankan test.

Contoh pemetaan:

- Duplikasi path -> bab 3 dan 12.
- Error diabaikan -> bab 7.
- Nama menyesatkan -> bab 2.
- Fungsi setup panjang -> bab 3 dan 10.
- Lock dipegang terlalu lama -> bab 13 dan appendix A.
- Test hanya sukses -> bab 9.
- Komentar usang -> bab 4.

## Referensi

- Buku: Robert C. Martin, Clean Code, Prentice Hall, 2008,
  ISBN 978-0-13-235088-4, appendix C.
- Daftar isi resmi:
  https://catdir.loc.gov/catdir/toc/ecip0820/2008024750.html
- Ringkasan bab 17:
  https://vivekkhatri.com/chapter-17-smells-heuristics-clean-code-robert-c-martin/
- Catatan bab 17:
  https://github.com/JuanCrg90/Clean-Code-Notes#chapter17
- Checklist internal:
  `.opencode/skills/opengema-cleancode/references/opengema-checklist.md`
