ENTRY(_start)

PHDRS
{
  image PT_LOAD FLAGS(7);
}

SECTIONS
{
  /* 1 TiB + 64 MiB: above the anonymous mmap cursor and clear of the main
     executable at 1 TiB. */
  . = 0x10004000000;
  .text : {
    KEEP(*(.text.entry))
    *(.text*)
  } :image
  .rodata : { *(.rodata*) } :image
  .data : { *(.data*) } :image
  .bss : { *(.bss*) *(COMMON) } :image
}
