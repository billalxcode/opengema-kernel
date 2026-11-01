# Bab 1 - Clean Code

Sumber: Clean Code, bab 1.

## Inti

Kode buruk menghambat perubahan dan pada akhirnya membunuh produk. Waktu
terbesar programmer bukan untuk menulis kode, tetapi membaca dan mengubah
kode. Karena itu kebersihan kode adalah tanggung jawab profesional, bukan
tambahan opsional.

Clean code berarti:

- Fokus dan sederhana.
- Mudah dibaca dan dipahami.
- Dites dengan baik.
- Tidak mengandung duplikasi.
- Menghormati pembaca berikutnya.

Bab ini juga memperkenalkan Boy Scout rule: tinggalkan kode lebih bersih
daripada saat ditemukan. Satu perbaikan kecil setiap kali menyentuh file.

## Aturan

- Jangan menambah kekotoran baru. Perbaiki satu hal saat melewatinya.
- Jangan menunda refactor sampai menjadi proyek besar.
- Jangan menerima "nanti saja" sebagai alasan permanen.
- Jangan memakai kode buruk sebagai warisan yang dibiarkan.
- Perubahan kecil yang teruji lebih baik daripada penulisan ulang besar.

## Penerapan di OpenGEMA

- Setiap perubahan harus membuat build dan test tetap hijau.
- Jika menyentuh file besar, perbaiki satu fungsi atau satu nama yang jelas
  salah, bukan seluruh file.
- Jangan menambah komentar "temporary" atau `TODO` tanpa pemilik.
- Jangan menambah global state baru tanpa alasan dan test.
- Jangan mencampur perubahan arsitektur dengan perbaikan kecil.
- Jika menemukan masalah besar, catat di `docs/CLEANCODE-REVIEW.md` dan
  tangani di commit terpisah.

## Pertanyaan review

- Apakah diff ini membuat kode lebih mudah dibaca?
- Apakah ada kekotoran baru yang bisa dihindari sekarang?
- Apakah perubahan ini terlalu besar untuk satu topik?
- Apakah test membuktikan perilaku yang berubah?
- Apakah ada `TODO` atau komentar yang menunda pekerjaan tanpa alasan?

## Referensi

- Buku: Robert C. Martin, Clean Code, Prentice Hall, 2008,
  ISBN 978-0-13-235088-4, bab 1.
- Daftar isi resmi:
  https://catdir.loc.gov/catdir/toc/ecip0820/2008024750.html
- O'Reilly bab 1:
  https://www.oreilly.com/library/view/clean-code-a/9780136083238/chapter01.xhtml
- Catatan bab 1:
  https://github.com/JuanCrg90/Clean-Code-Notes#chapter1
- Kritik bab 1:
  https://bugzmanov.github.io/cleancode-critique/chapter_1.html
- Ringkasan bab 1 dan 2:
  https://vuyisile.com/clean-code-chapters-1-2-summary/
