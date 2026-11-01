# Appendix A - Concurrency II

Sumber: Clean Code, appendix A.

## Inti

Appendix ini memperluas bab 13 dengan contoh dan pola concurrency yang
lebih rinci. Fokusnya adalah memahami jalur eksekusi, dependency antar
method, throughput, deadlock, dan cara menguji kode threaded.

Pelajaran utama:

- Ketahui execution model yang dipakai.
- Pahami possible paths dan interleaving.
- Ketahui library concurrency yang dipakai.
- Waspadai dependency antar method yang memegang lock.
- Ukur throughput dan latency, bukan hanya benar/salah.
- Deadlock bisa muncul dari urutan lock yang berbeda.
- Test concurrency harus bisa mengulang skenario sulit.

## Aturan

- Jangan memegang lock saat memanggil method lain yang juga mengambil lock
  tanpa aturan yang jelas.
- Dokumentasikan lock ordering.
- Hindari lock hierarki yang tidak perlu.
- Gunakan timeout atau mekanisme diagnosis untuk deadlock.
- Uji dengan beban dan jumlah CPU yang berbeda.
- Jangan menganggap test single core cukup.

## Penerapan di OpenGEMA

- `IrqSpinlock` dipakai di heap, PMM, VFS? dan virtio-blk.
- Jangan memanggil filesystem atau alokasi heap di bawah lock driver.
- `virtio-blk` sekarang menyerialkan read; pastikan tidak ada lock order
  dengan heap atau scheduler.
- Scheduler dan wait queue punya aturan sendiri.
- Test dengan `-smp 2` dan stress spawn/read.
- Catat lock ordering di komentar header jika melibatkan lebih dari satu
  lock.

## Pertanyaan review

- Apa execution model subsystem ini?
- Lock apa yang diambil dan dalam urutan apa?
- Apakah ada method yang memanggil method lain di bawah lock?
- Apakah test mencakup dua CPU?
- Apakah deadlock bisa didiagnosis?

## Referensi

- Buku: Robert C. Martin, Clean Code, Prentice Hall, 2008,
  ISBN 978-0-13-235088-4, appendix A.
- Daftar isi resmi:
  https://catdir.loc.gov/catdir/toc/ecip0820/2008024750.html
- Catatan bab 13 (terkait):
  https://github.com/JuanCrg90/Clean-Code-Notes#chapter13
- Ringkasan umum:
  https://gist.github.com/wojteklu/73c6914cc446146b8b533c0988cf8d29
