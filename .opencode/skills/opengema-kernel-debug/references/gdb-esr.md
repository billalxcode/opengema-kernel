# GDB dan ESR_EL1

## GDB

```sh
aarch64-none-elf-gdb build/BOOTAA64.EFI
(gdb) target remote :1234
(gdb) add-symbol-file build/BOOTAA64.EFI 0xaddr_loader
(gdb) b kmain
(gdb) c
(gdb) bt
(gdb) info registers
(gdb) x/8i $pc
(gdb) p/x $esr_el1
(gdb) p/x $far_el1
(gdb) p/x $elr_el1
```

## ESR decode cepat

- EC bits [31:26].
- `0x21` instr abort EL1.
- `0x24` data abort EL1.
- `0x0F` SVC.
- `0x3A` SError.
- DFSC bits [5:0]: `0b000100` L0, `0b000101` L1, `0b000110` L2, `0b000111` L3 translation fault. `0b001101` permission fault.

Panic harus cetak `ESR, FAR, ELR, SPSR, VBAR`.
