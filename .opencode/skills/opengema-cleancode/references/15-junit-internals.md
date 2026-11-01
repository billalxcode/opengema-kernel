# Bab 15 - JUnit Internals

Sumber: Clean Code, bab 15.

## Inti

Bab ini membedah kode JUnit sebagai contoh kode yang ditulis dengan baik.
Fokusnya bukan menghafal JUnit, tetapi belajar dari struktur, pemisahan
tanggung jawab, penamaan, dan test yang bisa dibaca.

Pelajaran utama:

- Kode framework yang baik memisahkan construction, execution, dan
  reporting.
- Nama yang baik membuat alur test mudah diikuti.
- Abstraksi kecil membantu variasi test.
- Komentar dipakai untuk kontrak, bukan menjelaskan kode.
- Test dan production code saling mencerminkan.

## Aturan

- Baca kode berkualitas sebagai latihan, bukan hanya menyalin pola.
- Jangan menambah framework test besar tanpa kebutuhan.
- Pisahkan runner, assertion, dan reporting.
- Buat test harness kecil dan bisa diperluas.
- Jangan mengunci test pada implementasi internal.

## Penerapan di OpenGEMA

- `scripts/test-host.sh` adalah harness host sederhana.
- `scripts/test-qemu.sh` adalah harness sistem.
- `scripts/uart_test_client.py` adalah driver test serial.
- `mkgslimg --verify` adalah verifier format.
- Jangan menambah dependency test framework yang tidak freestanding.
- Buat test helper kecil, misalnya `RunShellCommand`, `ExpectOutput`.
- Test harus bisa dijalankan di macOS host dan container.

## Pertanyaan review

- Apakah test harness memisahkan setup, aksi, dan assertion?
- Apakah nama test mudah dibaca?
- Apakah test bergantung pada detail internal?
- Apakah harness bisa diperluas tanpa duplikasi?
- Apakah ada bagian test yang bisa menjadi helper?

## Referensi

- Buku: Robert C. Martin, Clean Code, Prentice Hall, 2008,
  ISBN 978-0-13-235088-4, bab 15.
- Daftar isi resmi:
  https://catdir.loc.gov/catdir/toc/ecip0820/2008024750.html
- O'Reilly bab 15:
  https://www.oreilly.com/library/view/clean-code-a/9780136083238/chapter15.xhtml
- Catatan bab 15:
  https://github.com/JuanCrg90/Clean-Code-Notes#chapter15
- Ringkasan umum:
  https://gist.github.com/wojteklu/73c6914cc446146b8b533c0988cf8d29
