# Bab 14 - Successive Refinement

Sumber: Clean Code, bab 14.

## Inti

Bab ini adalah studi kasus penulisan parser argumen. Kode pertama biasanya
bekerja tetapi kotor. Penulis kemudian memperbaiki secara bertahap:
memisahkan tanggung jawab, menghapus duplikasi, memperbaiki nama, dan
menambah test.

Pelajaran utama:

- Jangan berhenti saat kode "sudah jalan".
- Perbaiki dalam langkah kecil.
- Jaga test tetap hijau selama refactor.
- Pisahkan parsing, validasi, dan eksekusi.
- Jangan takut membuang kode yang salah.
- Refactor adalah bagian dari pekerjaan, bukan proyek terpisah.

## Aturan

- Setiap langkah refactor harus kecil dan bisa diuji.
- Jangan mencampur refactor besar dengan fitur baru.
- Simpan test sebagai jaring pengaman.
- Hapus kode mati setelah yakin tidak dipakai.
- Perbaiki nama sebelum menambah komentar.
- Jika struktur salah, ubah struktur, bukan menambah lapisan.

## Penerapan di OpenGEMA

- Milestone GSL dikerjakan bertahap: layout, GSLFS, init, command layer.
- Setiap milestone menjaga `test.sh` hijau.
- `SetupWritableFs`, `run_external`, dan `gslfs::Mount` adalah kandidat
  successive refinement.
- Jangan menulis ulang seluruh kernel untuk satu fitur.
- Perbaiki satu fungsi atau satu modul per commit.
- Pertahankan kompatibilitas test serial dan QEMU.

## Pertanyaan review

- Apakah kode ini hanya "jalan" atau juga bersih?
- Apakah refactor dilakukan bertahap?
- Apakah test hijau di setiap langkah?
- Apakah ada struktur yang harus diubah, bukan ditambal?
- Apakah kode mati sudah dihapus?

## Referensi

- Buku: Robert C. Martin, Clean Code, Prentice Hall, 2008,
  ISBN 978-0-13-235088-4, bab 14.
- Daftar isi resmi:
  https://catdir.loc.gov/catdir/toc/ecip0820/2008024750.html
- O'Reilly bab 14:
  https://www.oreilly.com/library/view/clean-code-a/9780136083238/chapter14.xhtml
- Catatan bab 14:
  https://github.com/JuanCrg90/Clean-Code-Notes#chapter14
- Ringkasan umum:
  https://gist.github.com/wojteklu/73c6914cc446146b8b533c0988cf8d29
