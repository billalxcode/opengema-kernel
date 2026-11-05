# Lock Ordering

Dokumen ini mencatat urutan lock kernel. Aturannya konservatif: jika ragu,
jangan mengambil lock kedua saat lock pertama masih dipegang.

## Jenis lock

`arch::aarch64::IrqSpinlock` menyimpan DAIF per CPU, mematikan IRQ, lalu
mengambil spinlock. `unlock` mengembalikan DAIF CPU tersebut.

Konsekuensi:

- Lock ini tidak boleh dipakai untuk menunggu device dalam waktu lama.
- Lock ini tidak boleh diambil ulang oleh CPU yang sama (tidak rekursif).
- State yang dilindungi harus jelas dan kecil.

## Urutan yang diketahui

Urutan panah berarti lock kiri boleh diambil saat lock kanan belum dipegang,
dan lock kanan adalah leaf.

```
heap    -> PMM
sched   -> PMM
virtio  -> leaf
```

Penjelasan:

- `heap::g_lock` boleh memanggil PMM saat slab baru dibutuhkan.
- `sched::g_lock` boleh memanggil PMM saat stack thread baru dialokasikan.
- `virtio-blk` queue lock adalah leaf: tidak boleh memanggil heap, PMM,
  filesystem, atau scheduler di dalamnya.
- PMM tidak boleh memanggil heap atau scheduler.
- Jangan mengambil `heap::g_lock` atau `sched::g_lock` saat PMM lock
  dipegang.

## Aturan praktis

1. Jangan memegang lock saat memanggil `HeapAlloc`, `HeapFree`, atau operasi
   filesystem, kecuali lock tersebut memang lock alokator.
2. Jangan memegang lock driver saat menunggu interrupt atau polling device
   yang panjang.
3. Jangan memanggil `ThreadYield`, `ThreadSleep`, atau `WaitQueueWait` saat
   memegang `sched::g_lock`.
4. Jangan memanggil `AddressSpaceActivate` atau `SwitchContext` saat masih
   memegang `sched::g_lock`; scheduler sudah melepas lock sebelum switch.
5. Jika perlu dua lock, tulis urutannya di komentar fungsi dan tambahkan
   test stress di QEMU.
6. Lock baru harus punya nama, pemilik state, dan alasan di komentar header.

## Lock yang ada

| Lock | Modul | Melindungi | Catatan |
|---|---|---|---|
| `heap::g_lock` | `kernel/mem/heap.cpp` | free list dan slab | boleh memanggil PMM |
| `pmm::g_lock` | `kernel/mem/pmm.cpp` | bitmap/frame allocator | leaf untuk alokasi |
| `sched::g_lock` | `kernel/sched/scheduler.cpp` | run queue, thread state | boleh memanggil PMM |
| `Blk::lock_` | `kernel/drivers/virtio/virtio_blk.cpp` | queue dan request buffer | leaf |
| `gslfs` | `kernel/fs/gslfs.cpp` | tidak ada lock | mount sekali, read-only |
| `vfs` | `kernel/fs/vfs.cpp` | mount table | leaf; tidak boleh memanggil operasi filesystem di dalam lock |

## Risiko yang belum ditutup

- Belum ada lock ordering checker otomatis.
- Belum ada stress test konkurensi di QEMU setelah refactor.
- `sched::g_lock` dan `heap::g_lock` belum pernah diuji bersamaan di SMP
  dengan beban tinggi.
