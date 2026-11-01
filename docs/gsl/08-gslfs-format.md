# 08 - Format GSLFS

GSLFS adalah filesystem read-only untuk volume system GSL. Format ini sudah
dipakai oleh `scripts/mkgslimg.py` dan driver `kernel/fs/gslfs.cpp`.

## 1. Prinsip

1. Read-only saat runtime.
2. Semua metadata dan data punya checksum CRC32.
3. Ukuran dan offset divalidasi sebelum dipakai.
4. File disimpan contiguous.
5. Tidak ada alokasi atau perubahan saat mount.
6. Tool pembuat image berjalan di host dan deterministik.

## 2. Layout image

```
+------------------+  offset 0
| Superblock       |  4096 byte
+------------------+  node_offset (4096)
| Node table       |  node_count * 56 byte, dipad ke block
+------------------+  name_offset
| Name table       |  nama NUL, dipad ke block
+------------------+  data_offset
| Data blocks      |  data file contiguous
+------------------+
```

Semua angka little-endian. Semua offset dihitung dari awal image.
`block_size` adalah 4096.

## 3. Superblock

```
offset  ukuran  field
0       8       magic = "GSLFS1\0\0"
8       4       version = 1
12      4       block_size = 4096
16      8       block_count
24      4       node_count
28      4       root_node
32      8       node_offset
40      8       name_offset
48      8       data_offset
56      4       checksum
60      4036    reserved, nol
```

`checksum` adalah CRC32 dari byte 0 sampai 55. Sisa superblock sampai 4096
byte harus nol.

Mount gagal jika magic, versi, block size, ukuran, offset, atau checksum
tidak cocok.

## 4. Node

Setiap file atau directory adalah satu node 56 byte.

```
offset  ukuran  field
0       4       type        0 file, 1 directory
4       4       mode        permission Unix
8       4       uid
12      4       gid
16      4       parent      index node parent
20      4       name_offset offset dari awal name table
24      8       size        ukuran file
32      8       data_block  block awal data, absolute
40      8       block_count jumlah block data
48      4       checksum    CRC32 byte 0..47
52      4       data_checksum CRC32 isi file
```

Aturan:

- `parent` root adalah dirinya sendiri.
- `parent` node lain harus menunjuk directory.
- `name_offset` menunjuk NUL-terminated name di name table.
- Name maksimum 63 byte karena VFS memakai buffer segmen 64 byte.
- Name tidak boleh kosong (kecuali root) dan tidak boleh mengandung `/`.
- Directory harus `size = 0` dan `block_count = 0`.
- File kosong boleh `block_count = 0`.
- `data_block` adalah index block absolute di image, bukan relatif ke
  `data_offset`.
- `block_count` harus sama dengan `ceil(size / block_size)`.
- `data_checksum` untuk file kosong adalah 0.

`node_count` maksimum 128 untuk implementasi kernel saat ini.

## 5. Name table

Name disimpan berurutan sesuai urutan node, masing-masing diakhiri NUL.
Root memakai name kosong. Name table dipad dengan nol sampai batas block.

`name_offset` pada node adalah offset relatif dari awal name table, bukan
offset absolute image.

## 6. Data

- Data file contiguous, dimulai pada `data_block * block_size`.
- Offset dan ukuran divalidasi terhadap `block_count` image.
- Block data dipad dengan nol.
- Driver memverifikasi checksum setiap file saat mount.
- File besar nanti bisa diverifikasi bertahap, tetapi untuk image system
  sekarang verifikasi saat mount masih murah.

## 7. Permission

Field `mode` memakai bit Unix:

```
0400 read owner
0200 write owner
0100 execute owner
0040 read group
0004 read other
```

Volume read-only menolak operasi tulis di VFS. Bit write tetap disimpan
agar metadata bisa dipakai saat image diperiksa di host atau dipindahkan.

## 8. Mount

Saat mount, kernel:

1. Membaca superblock dan memvalidasi checksum.
2. Memvalidasi `block_count`, `node_count`, `root_node`, dan offset.
3. Membaca setiap node dan memvalidasi checksum metadata.
4. Membaca name table dan memvalidasi nama.
5. Memvalidasi parent, tipe, dan reachability ke root.
6. Memverifikasi checksum data setiap file.
7. Membangun node VFS statis.

Jika salah satu langkah gagal, mount gagal dan tidak ada node yang dipakai.

## 9. Tool mkgslimg

```sh
python3 scripts/mkgslimg.py build/system-root build/gema-system.img
python3 scripts/mkgslimg.py --verify build/gema-system.img
```

Aturan:

- Node diurutkan berdasarkan path.
- Root adalah node 0.
- Data mengikuti urutan node.
- Tidak ada timestamp di image, sehingga hasil deterministik.
- Symlink ditolak.
- Mode diambil dari file host.
- Tool menghitung semua checksum.
- `--verify` membaca ulang image dan memeriksa semua checksum.

## 10. Partisi system

Image GSLFS ditulis ke partisi kedua disk boot:

```
partisi 1  ESP FAT32     32 MiB
partisi 2  GSLFS         32 MiB
```

Kernel mencari partisi dengan membaca awal setiap partisi GPT dan mencocokkan
magic `GSLFS1\0\0`. Partisi ESP tetap dipakai UEFI dan di-mount kernel di
`/volumes/boot` setelah root GSLFS aktif.

## 11. Update system

Karena image read-only:

- Update dilakukan dengan membuat image baru.
- Boot dapat memilih partisi A atau B yang valid.
- Jika image baru gagal validasi, boot kembali ke image lama.
- Data user di `/users`, `/etc`, dan `/var` tidak ikut diganti.

## 12. Batas awal

- Name maksimum 63 byte.
- Node maksimum 128.
- Tidak ada hard link.
- Tidak ada symlink.
- Tidak ada compression.
- Tidak ada journal atau alokasi runtime.

## 13. Yang masih dibahas

- Dukungan symlink untuk `/commands`.
- Checksum per blok untuk file besar.
- Versi format berikutnya dan aturan kompatibilitas.
- Update A/B dan rollback.
