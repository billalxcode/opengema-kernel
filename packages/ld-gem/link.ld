ENTRY(_start)

PHDRS
{
  text PT_LOAD FLAGS(5);
  data PT_LOAD FLAGS(6);
}

SECTIONS
{
  /* 1 TiB + 64 MiB: above the anonymous mmap cursor and clear of the main
     executable at 1 TiB. */
  . = 0x10004000000;
  .text : {
    KEEP(*(.text.entry))
    *(.text*)
  } :text
  .rodata : { *(.rodata*) } :text
  .rela.dyn : { *(.rela.dyn) } :text

  . = ALIGN(0x1000);
  .data : { *(.data*) } :data
  .got : { *(.got*) } :data
  .init_array : {
    __init_array_start = .;
    KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
    KEEP(*(.init_array))
    __init_array_end = .;
  } :data
  .bss : { *(.bss*) *(COMMON) } :data
}
