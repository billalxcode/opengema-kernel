---
name: opengema-cleancode
description: Best practices from Robert C. Martin's Clean Code for opengema-kernel. Use when writing, reviewing, or refactoring C++20 kernel and GSL userland code, naming, functions, comments, error handling, tests, classes, systems, concurrency, or code smells.
---

# opengema-cleancode

Sumber utama: Robert C. Martin, Clean Code: A Handbook of Agile Software
Craftsmanship, Prentice Hall, 2008, ISBN 978-0-13-235088-4.

Skill ini menerjemahkan prinsip Clean Code ke opengema-kernel. Prinsipnya
umum, tetapi aturan bahasa dan arsitektur tetap mengikuti skill
`opengema-kernel-styleguide`, `opengema-kernel-cxx`, dan `docs/gsl`.

## Kapan dipakai

- Menulis atau mengubah kode kernel dan userland.
- Review diff atau pull request.
- Refactor kode yang sulit dibaca atau diuji.
- Menamai tipe, fungsi, variabel, file, dan test.
- Menulis test baru atau memperbaiki test lama.
- Mendiagnosis error handling, boundary, class, system, atau concurrency.

## Aturan yang tidak boleh dilanggar

1. Build dan test harus hijau sebelum dan sesudah perubahan.
2. Satu diff satu topik. Jangan campur driver, MMU, dan build.
3. Ikuti arah layer GSL. Aplikasi hanya memakai `libgemapi`.
4. Hanya `libkgema` yang boleh berisi instruksi `svc`.
5. Operasi fallible mengembalikan `Status` atau `Result`; jangan abaikan
   error.
6. Tanpa exception, RTTI, atau alokasi tersembunyi.
7. Komentar menjelaskan why, bukan what.
8. Tidak ada em dash atau Unicode non-keyboard.
9. Nama mengikuti styleguide proyek: tipe `PascalCase`, free function
   `PascalCase`, method `snake_case`, variabel `snake_case`, konstanta
   `kPascalCase`.
10. Test jalur gagal sama pentingnya dengan test jalur sukses.

## Alur review

1. Baca diff dan tujuannya.
2. Pilih bab yang relevan dari tabel di bawah.
3. Baca `references/<nomor>-<nama>.md`.
4. Terapkan checklist di `references/opengema-checklist.md`.
5. Perbaiki masalah terkecil yang memberi dampak terbesar.
6. Jalankan:
   - `./scripts/check-gsl.sh`
   - `./scripts/check-arch.sh`
   - `./scripts/test.sh`
7. Catat temuan yang sengaja ditunda di `docs/CLEANCODE-REVIEW.md`.

## Peta bab

| Bab | Fokus | Reference |
|---|---|---|
| 1 | Clean code, biaya kotor, Boy Scout rule | `01-clean-code.md` |
| 2 | Nama yang mengungkap maksud | `02-meaningful-names.md` |
| 3 | Fungsi kecil, satu tugas, sedikit argumen | `03-functions.md` |
| 4 | Komentar yang berguna, bukan pengganti kode | `04-comments.md` |
| 5 | Formatting vertikal dan horizontal | `05-formatting.md` |
| 6 | Object, data structure, Law of Demeter | `06-objects-and-data-structures.md` |
| 7 | Error handling dengan Status/Result | `07-error-handling.md` |
| 8 | Boundary, adapter, dan kode pihak ketiga | `08-boundaries.md` |
| 9 | Unit test, TDD, dan F.I.R.S.T. | `09-unit-tests.md` |
| 10 | Class kecil, kohesi, SRP | `10-classes.md` |
| 11 | System, construction, dependency | `11-systems.md` |
| 12 | Emergent design dan empat aturan sederhana | `12-emergence.md` |
| 13 | Concurrency, shared state, deadlock | `13-concurrency.md` |
| 14 | Successive refinement | `14-successive-refinement.md` |
| 15 | Pelajaran desain dari JUnit | `15-junit-internals.md` |
| 16 | Refactoring SerialDate | `16-refactoring-serialdate.md` |
| 17 | Smells dan heuristics | `17-smells-and-heuristics.md` |
| A | Concurrency II | `appendix-a-concurrency-ii.md` |
| B | org.jfree.date.SerialDate | `appendix-b-serialdate.md` |
| C | Cross reference heuristics | `appendix-c-heuristics-cross-reference.md` |

## OpenGEMA yang harus dijaga

- Kernel hanya menyentuh hardware. Kebijakan user ada di user space.
- Layout filesystem mutlak di `packages/libgsl`.
- `ld-gem` adalah satu-satunya program statis jangka panjang.
- `libgemapi` adalah satu-satunya API aplikasi.
- Shared state di kernel harus punya lock dan alasan.
- Setiap alokasi harus punya jalur pembebasan yang diuji.
- Setiap driver harus punya test jalur sukses dan jalur gagal.
- Setiap syscall harus memvalidasi pointer, panjang, dan hak akses.

## Definisi selesai

- Perubahan kecil dan fokus.
- Nama menjelaskan maksud tanpa komentar.
- Fungsi pendek dan punya satu alasan untuk berubah.
- Error tidak hilang diam-diam.
- Test baru mencakup jalur sukses dan minimal satu jalur gagal.
- `check-gsl`, `check-arch`, dan `test.sh` hijau.
- Tidak ada em dash atau Unicode non-keyboard.

## References

- `references/sources.md` untuk bibliografi dan URL.
- `references/opengema-checklist.md` untuk checklist review cepat.
- `docs/CLEANCODE-REVIEW.md` untuk hasil penerapan di repo ini.
