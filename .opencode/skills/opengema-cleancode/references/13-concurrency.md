# Bab 13 - Concurrency

Sumber: Clean Code, bab 13.

## Inti

Concurrency sulit karena shared state, timing, dan nondeterminism. Kode
concurrent harus punya tanggung jawab yang jelas, shared state minimum, dan
test yang benar-benar menjalankan parallelisme.

Prinsip utama:

- Concurrency adalah tanggung jawab tersendiri.
- Batasi scope data yang dibagi.
- Gunakan salinan data bila memungkinkan.
- Buat thread independen.
- Ketahui execution model: thread pool, producer-consumer, readers-writers.
- Waspadai shared state dan race.
- Test threaded code dengan banyak skenario.
- Hindari deadlock, starvation, dan livelock.

## Aturan

- Jangan memegang lock saat blocking I/O atau alokasi besar.
- Urutan lock harus konsisten.
- Jangan memanggil kode external di bawah lock tanpa alasan.
- Jangan mengakses state global tanpa lock atau per-CPU.
- Jangan mengandalkan timing test.
- Jangan menambah thread tanpa batas resource.
- Inisialisasi lock sebelum dipakai dan setelah zeroing.

## Penerapan di OpenGEMA

- SMP dua CPU di QEMU adalah test default.
- `IrqSpinlock` melindungi state yang dipakai lintas CPU.
- Heap dan PMM punya lock sendiri.
- `virtio-blk` memakai satu queue dan request buffer; akses harus
  diserialisasi. Ini pernah menyebabkan kegagalan baca GSLFS saat spawn
  paralel.
- Data per CPU dipakai untuk `SP_EL0` dan state interrupt.
- Jangan memegang lock saat menunggu device jika bisa dihindari.
- TLB shootdown dan migrasi thread harus benar sebelum migrasi diaktifkan.
- Test negatif: dua spawn bersamaan, dua proses membaca file bersamaan,
  stress `wait`/`exit`.

## Pertanyaan review

- Data apa yang dibagi?
- Lock apa yang melindunginya?
- Apakah urutan lock konsisten?
- Apakah lock dipegang saat blocking?
- Apakah test benar-benar parallel?
- Apakah ada asumsi single core?

## Referensi

- Buku: Robert C. Martin, Clean Code, Prentice Hall, 2008,
  ISBN 978-0-13-235088-4, bab 13.
- Daftar isi resmi:
  https://catdir.loc.gov/catdir/toc/ecip0820/2008024750.html
- O'Reilly bab 13:
  https://www.oreilly.com/library/view/clean-code-a/9780136083238/chapter13.xhtml
- Catatan bab 13:
  https://github.com/JuanCrg90/Clean-Code-Notes#chapter13
- Ringkasan umum:
  https://gist.github.com/wojteklu/73c6914cc446146b8b533c0988cf8d29
