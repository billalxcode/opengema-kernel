# Bab 9 - Unit Tests

Sumber: Clean Code, bab 9.

## Inti

Test code adalah kode produksi. Test yang kotor membuat perubahan menjadi
lambat dan rapuh. Test harus dijaga sebersih kode aplikasi.

Tiga hukum TDD:

1. Jangan menulis kode produksi sebelum ada test yang gagal.
2. Jangan menulis test lebih dari yang cukup untuk gagal.
3. Jangan menulis kode produksi lebih dari yang cukup untuk lulus.

Prinsip test bersih:

- Satu assert per test jika memungkinkan.
- Satu konsep per test.
- F.I.R.S.T.: Fast, Independent, Repeatable, Self-validating, Timely.
- Nama test menjelaskan perilaku dan kondisi.
- Test tidak bergantung pada urutan.
- Test memakai data yang deterministik.
- Test reading seperti spesifikasi.

## Aturan

- Test harus bisa dijalankan ulang tanpa state lama.
- Jangan memakai sleep untuk sinkronisasi jika bisa dihindari.
- Jangan menyalin logika produksi ke test.
- Jangan membuat test yang selalu lulus.
- Uji boundary, error, dan empty input.
- Test yang flaky harus diperbaiki, bukan diabaikan.

## Penerapan di OpenGEMA

- `scripts/test-host.sh` untuk unit test murni.
- `scripts/test-qemu.sh` untuk smoke test end-to-end.
- `scripts/check-arch.sh` dan `scripts/check-gsl.sh` untuk aturan layer.
- Test negatif: file non-ELF, pointer invalid, descriptor salah, checksum
  GSLFS salah, library hilang.
- Test concurrency harus dijalankan dengan SMP 2 CPU.
- Frame balance dan `process: remaining: 0` adalah invariant test.
- GSLFS `mkgslimg --verify` adalah test format.
- Test baru harus deterministik dan tidak menambah waktu boot berlebihan.

## Pertanyaan review

- Apakah test membuktikan satu perilaku?
- Apakah test independen dan repeatable?
- Apakah ada jalur gagal yang diuji?
- Apakah test memakai data deterministik?
- Apakah test membaca seperti spesifikasi?
- Apakah test akan menangkap regresi yang relevan?

## Referensi

- Buku: Robert C. Martin, Clean Code, Prentice Hall, 2008,
  ISBN 978-0-13-235088-4, bab 9.
- Daftar isi resmi:
  https://catdir.loc.gov/catdir/toc/ecip0820/2008024750.html
- O'Reilly bab 9:
  https://www.oreilly.com/library/view/clean-code-a/9780136083238/chapter09.xhtml
- Catatan bab 9:
  https://github.com/JuanCrg90/Clean-Code-Notes#chapter9
- Ringkasan bab 9:
  https://github.com/thebentern/clean-code-study/blob/master/Chapter-9.md
- Ringkasan tambahan:
  https://www.linkedin.com/pulse/summary-clean-code-chapter-9-unit-tests-robert-c-martin-el-mhamdi
