# 08 - Format GSLFS

GSLFS adalah filesystem read-only khusus untuk volume system GSL. Tujuannya
sederhana, mudah divalidasi, dan tidak membutuhkan journal.

## 1. Prinsip

1. Read-only saat runtime.
2. Semua metadata dan data punya checksum.
3. Ukuran dan offset divalidasi sebelum dipakai.
4. File disimpan berurutan di image.
5. Tidak ada alokasi atau perubahan saat mount.
6. Tool pembuat image berjalan di host.

## 2. Layout image

```
+------------------+
| Superblock       |
+------------------+
| Node table       |
+------------------+
| Name table       |
+------------------+
| Data blocks      |
+------------------+
```

Semua angka little-endian dan semua offset dihitung dari awal image.

## 3. Superblock

Usulan field:

```
magic        8 byte   "GSLFS1\0"
version      u32
block_size   u32      default 4096
block_count  u64
node_count   u32
root_node    u32
node_offset  u64
name_offset  u64
data_offset  u64
checksum     u32      CRC32 isi superblock
```

Mount gagal jika magic, versi, ukuran, atau checksum tidak cocok.

## 4. Node

Setiap file atau directory adalah satu node.

```
type         u32      0 file, 1 directory, 2 symlink nanti
mode         u32      permission dan executable
uid          u32
gid          u32
parent       u32      index node parent
name_offset  u32      offset ke name table
size         u64
data_block   u64      block awal
block_count  u64
checksum     u32      CRC32 metadata
data_checksum u32     CRC32 isi file
```

Aturan:

- `parent` untuk root adalah dirinya sendiri.
- Name tidak boleh kosong, tidak boleh mengandung `/`.
- File harus memiliki range data yang valid.
- Directory tidak memiliki data.
- Symlink ditunda sampai ada kebutuhan nyata.

## 5. Name table

Name disimpan sebagai byte ASCII atau UTF-8 dengan null terminator.

- Panjang maksimum 255 byte.
- Path tidak disimpan penuh; lookup berjalan dari root atau cache.
- Name table read-only dan tidak pernah diubah.

## 6. Data

- Data file berurutan dan contiguous.
- Offset dan ukuran divalidasi terhadap `block_count`.
- File kosong boleh punya `block_count = 0`.
- Data checksum diverifikasi saat file dibuka atau saat mount jika ukuran
  image masih kecil.
- Untuk file besar, verifikasi saat load atau baca bertahap.

## 7. Permission

Field `mode` memakai bit yang mirip Unix:

```
0400 read owner
0200 write owner
0100 execute owner
0040 read group
0004 read other
```

Karena volume read-only, bit write tidak berpengaruh dan operasi tulis
ditolak. Bit executable dipakai loader untuk menolak binary tanpa izin.

## 8. Mount

Saat mount, kernel:

1. Membaca superblock dan memvalidasi checksum.
2. Memvalidasi node table dan name table.
3. Membangun root node di VFS.
4. Menolak image jika ada offset atau range yang tidak valid.

Tidak ada alokasi node dinamis. Node VFS menunjuk langsung ke node GSLFS.

## 9. Tool mkgslimg

Tool host `mkgslimg` membuat image dari direktori:

```
mkgslimg packages/system-root build/gema-system.img
```

Aturan:

- Urutan node deterministik agar image bisa direproduksi.
- Node directory dan file mengikuti tree.
- File di luar direktori root ditolak.
- Permission diambil dari mode file host atau manifest.
- Tool menolak symlink sampai format mendukungnya.
- Tool menghitung checksum.

## 10. Update system

Karena image read-only:

- Update dilakukan dengan membuat image baru.
- Boot memilih volume A atau B yang valid.
- Jika image baru gagal validasi, boot kembali ke image lama.
- Data user tidak ikut diganti.

## 11. Batas awal

- Nama maksimum 255 byte.
- Node maksimum ditentukan versi, misalnya 4096 untuk image awal.
- File maksimum mengikuti ukuran image.
- Tidak ada hard link.
- Tidak ada symlink sampai disepakati.
- Tidak ada compression.

## 12. Yang masih dibahas

- CRC32 atau checksum lain.
- Checksum per blok atau per file.
- Apakah mode permission diambil dari host atau manifest.
- Apakah image mendukung banyak volume dalam satu file.
- Apakah perlu index directory untuk lookup cepat.
