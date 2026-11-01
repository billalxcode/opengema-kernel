# init

Proses PID 1 user space untuk GSL. Program ini dijalankan kernel dari
`/system/boot/init`.

Saat ini `init` menjalankan shell dari
`/applications/gemabox/gemabox` dengan argumen `shell`, lalu menunggu dan
meneruskan exit status shell.

## Rencana

- Memastikan mount tetap `/etc`, `/users`, `/var`, dan `/tmp` tersedia.
- Menjalankan service manager minimal.
- Menjalankan shell setelah service siap.
- Recovery shell jika shell atau service gagal.
- Restart service yang crash.

## Build

CMake menghasilkan `build/init.elf`, lalu menyalinnya ke
`build/system-root/system/boot/init` dan memasukkannya ke image GSLFS.

`init` adalah ELF dynamic dengan `PT_INTERP /system/boot/ld-gem` dan
`DT_NEEDED libgemapi.so.1`.
