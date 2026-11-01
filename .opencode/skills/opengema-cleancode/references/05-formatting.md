# Bab 5 - Formatting

Sumber: Clean Code, bab 5.

## Inti

Formatting adalah komunikasi. Kode yang rapi memudahkan pembaca melihat
struktur, alur, dan hubungan antar bagian. Formatting bukan selera pribadi
di dalam tim; harus ada aturan tim yang konsisten.

Prinsip vertikal:

- Newspaper metaphor: bagian penting di atas, detail di bawah.
- Openness and density: baris kosong memisahkan konsep, baris rapat
  menunjukkan hubungan.
- Vertical distance: deklarasi dekat dengan pemakaian.
- Dependent functions: pemanggil dekat dengan yang dipanggil.
- Conceptual affinity: hal yang mirip berdekatan.
- Vertical ordering: urutan dari umum ke detail.

Prinsip horizontal:

- Lebar baris wajar, tidak perlu scroll horizontal.
- Alignment yang membantu, bukan alignment berlebihan.
- Indentation menunjukkan scope dan hirarki.
- Dummy scopes dihindari.
- Team rules mengalahkan preferensi pribadi.

## Aturan

- Gunakan formatter otomatis, bukan perdebatan manual.
- Jangan mencampur tab dan spasi.
- Satu pernyataan per baris.
- Kelompokkan include, konstanta, tipe, dan fungsi secara konsisten.
- File yang terlalu panjang adalah sinyal untuk memecah tanggung jawab.
- Jangan memakai alignment kolom yang rapuh.

## Penerapan di OpenGEMA

- `clang-format` berbasis LLVM, `ColumnLimit: 100`, `IndentWidth: 2`,
  `PointerAlignment: Left`.
- `#pragma once` di baris pertama header.
- Urutan include: own header, C++ freestanding, C, arch, project.
- Satu unit `.h` + `.cpp` untuk modul kernel.
- Header hanya deklarasi dan `constexpr`; implementasi besar di `.cpp`.
- Formatting bukan alasan untuk tidak memecah fungsi panjang.
- Jangan memformat ulang seluruh file dalam commit fitur.

## Pertanyaan review

- Apakah file mengikuti clang-format?
- Apakah include dan deklarasi terkelompok dengan jelas?
- Apakah ada baris terlalu panjang atau alignment rapuh?
- Apakah file terlalu besar untuk satu tanggung jawab?
- Apakah diff formatting tercampur dengan perubahan logika?

## Referensi

- Buku: Robert C. Martin, Clean Code, Prentice Hall, 2008,
  ISBN 978-0-13-235088-4, bab 5.
- Daftar isi resmi:
  https://catdir.loc.gov/catdir/toc/ecip0820/2008024750.html
- O'Reilly bab 5:
  https://www.oreilly.com/library/view/clean-code-a/9780136083238/chapter05.xhtml
- Catatan bab 5:
  https://github.com/JuanCrg90/Clean-Code-Notes#chapter5
- Panduan style internal:
  `.opencode/skills/opengema-kernel-styleguide/references/clang-format-tidy.md`
