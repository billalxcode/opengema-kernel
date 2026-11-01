# Bab 6 - Objects and Data Structures

Sumber: Clean Code, bab 6.

## Inti

Object menyembunyikan data dan mengekspos perilaku. Data structure
mengekspos data dan tidak punya perilaku berarti. Keduanya berguna, tetapi
jangan dicampur menjadi hybrid yang membocorkan detail sekaligus
menyulitkan perubahan.

Prinsip utama:

- Data abstraction: expose perilaku, bukan representasi internal.
- Data/object anti-symmetry: prosedural cocok untuk data structure baru;
  OO cocok untuk tipe baru tanpa mengubah operasi lama.
- Law of Demeter: jangan merangkai getter menjadi train wreck.
- Hindari hybrid setengah object, setengah struct.
- Hiding structure: jangan biarkan pemanggil menavigasi internal.
- DTO adalah struct data murni tanpa logika.
- Active record adalah DTO dengan akses database; jangan menambah logika
  domain ke dalamnya.

## Aturan

- Jika pemanggil butuh beberapa getter untuk satu tugas, pindahkan tugas
  itu ke object.
- Jangan mengekspos pointer internal yang bisa dimutasi.
- Bedakan data murni (POD) dan object dengan invariant.
- Jangan memakai struct besar sebagai god object.
- Jangan menambahkan method ke DTO hanya agar terlihat OO.

## Penerapan di OpenGEMA

- `AddressSpace`, `DirEntry`, `Stat`, `BlockDevice`, dan `Image` adalah
  data structure dengan field publik. Itu wajar untuk kernel.
- `Blk`, `Linker`, `File`, dan `Spinlock` adalah object dengan invariant
  dan operasi.
- `NodeOps` adalah tabel fungsi; pemanggil tidak perlu tahu detail
  filesystem.
- `Node.fs_data` adalah opaque pointer. Jangan casting di luar driver.
- `Result<T>` menyembunyikan nilai dan status; jangan mengintip field.
- Jangan mengekspos layout internal GSLFS ke aplikasi; gunakan API.
- Jangan menambahkan getter setter trivial ke struct kernel.

## Pertanyaan review

- Apakah tipe ini object atau data structure?
- Apakah invariant dilindungi?
- Apakah ada train wreck `a.b().c().d()`?
- Apakah pemanggil dipaksa tahu struktur internal?
- Apakah ada hybrid yang membocorkan detail?

## Referensi

- Buku: Robert C. Martin, Clean Code, Prentice Hall, 2008,
  ISBN 978-0-13-235088-4, bab 6.
- Daftar isi resmi:
  https://catdir.loc.gov/catdir/toc/ecip0820/2008024750.html
- O'Reilly bab 6:
  https://www.oreilly.com/library/view/clean-code-a/9780136083238/chapter06.xhtml
- Catatan bab 6:
  https://github.com/JuanCrg90/Clean-Code-Notes#chapter6
- Ringkasan umum:
  https://gist.github.com/wojteklu/73c6914cc446146b8b533c0988cf8d29
