# Bab 10 - Classes

Sumber: Clean Code, bab 10.

## Inti

Class harus kecil, punya satu tanggung jawab, dan kohesif. Ukuran diukur
dari jumlah tanggung jawab, bukan jumlah baris. Class yang berubah karena
banyak alasan adalah god object.

Prinsip utama:

- Single Responsibility Principle.
- Cohesion: method dan data saling terkait.
- Organize for change: pisahkan hal yang berubah bersama.
- Isolate from change: depend pada abstraksi, bukan detail.
- Dependency Inversion: detail bergantung pada abstraksi.
- Interface kecil lebih baik daripada interface besar.
- Class harus mudah dites secara terpisah.

## Aturan

- Jika class punya banyak alasan untuk berubah, pecah.
- Jika method tidak memakai field, mungkin class terlalu besar.
- Jangan membuat class hanya untuk satu getter.
- Jangan mencampur I/O, parsing, dan kebijakan dalam satu class.
- Gunakan composition daripada inheritance jika tidak ada is-a.
- Jangan mengekspos mutable state.

## Penerapan di OpenGEMA

- `Blk` menangani virtio-blk dan queue; bukan filesystem.
- `Linker` menangani symbol scope, relokasi, dan initializer.
- `File` memiliki descriptor dan menutupnya otomatis.
- `AddressSpace` adalah data structure; operasi ada di VMM.
- `Process` memiliki state proses; scheduler dan VFS dipisah.
- `GslfsNode` adalah node filesystem; driver GSLFS memiliki operasinya.
- `NodeOps` memisahkan operasi filesystem dari struktur node.
- Jika `SetupWritableFs` terus tumbuh, pecah menjadi class atau fungsi
  kecil dengan tanggung jawab jelas.
- Jangan membuat `Manager` atau `Helper` tanpa tanggung jawab spesifik.

## Pertanyaan review

- Apa satu tanggung jawab class ini?
- Apakah semua method memakai state yang sama?
- Apakah class berubah karena alasan yang berbeda-beda?
- Apakah dependency mengarah ke abstraksi?
- Apakah class bisa dites tanpa hardware?

## Referensi

- Buku: Robert C. Martin, Clean Code, Prentice Hall, 2008,
  ISBN 978-0-13-235088-4, bab 10.
- Daftar isi resmi:
  https://catdir.loc.gov/catdir/toc/ecip0820/2008024750.html
- O'Reilly bab 10:
  https://www.oreilly.com/library/view/clean-code-a/9780136083238/chapter10.xhtml
- Catatan bab 10:
  https://github.com/JuanCrg90/Clean-Code-Notes#chapter10
- Ringkasan umum:
  https://gist.github.com/wojteklu/73c6914cc446146b8b533c0988cf8d29
