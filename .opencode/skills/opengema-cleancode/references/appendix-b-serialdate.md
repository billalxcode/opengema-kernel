# Appendix B - org.jfree.date.SerialDate

Sumber: Clean Code, appendix B.

## Inti

Appendix ini memuat kode sumber `SerialDate` yang dipakai sebagai bahan
studi kasus pada bab 16. Tujuannya adalah melihat kode asli sebelum
refactor, mengenali smell, dan membandingkan dengan versi yang lebih
bersih.

Pelajaran utama:

- Kode lama sering memiliki nama yang menyesatkan.
- Tanggung jawab tanggal, kalender, dan format sering tercampur.
- Magic number dan komentar usang menyulitkan perubahan.
- Test karakterisasi diperlukan sebelum refactor.
- Perbandingan sebelum/sesudah adalah alat belajar yang kuat.

## Aturan

- Jangan menyalin kode contoh ke produksi tanpa adaptasi.
- Gunakan kode ini sebagai studi, bukan template.
- Jika mengadaptasi ide, tulis ulang dengan tipe dan test proyek.
- Jangan menambah kompleksitas kalender tanpa kebutuhan nyata.

## Penerapan di OpenGEMA

- Gunakan sebagai latihan review untuk kode waktu/RTC.
- Bandingkan dengan `kernel/drivers/rtc` dan konversi waktu.
- Identifikasi magic number dan komentar usang.
- Buat test karakterisasi sebelum mengubah format waktu.
- Jangan menaruh logika kalender di driver.

## Pertanyaan review

- Apakah kode waktu kita punya tanggung jawab yang tercampur?
- Apakah ada magic number tanggal?
- Apakah test karakterisasi ada?
- Apakah komentar masih benar?
- Apakah refactor bisa dilakukan bertahap?

## Referensi

- Buku: Robert C. Martin, Clean Code, Prentice Hall, 2008,
  ISBN 978-0-13-235088-4, appendix B.
- Daftar isi resmi:
  https://catdir.loc.gov/catdir/toc/ecip0820/2008024750.html
- Bab 16 terkait:
  https://www.oreilly.com/library/view/clean-code-a/9780136083238/chapter16.xhtml
- Catatan bab 16:
  https://github.com/JuanCrg90/Clean-Code-Notes
