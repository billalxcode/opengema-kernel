ENTRY(_start)

PHDRS
{
  text PT_LOAD FLAGS(5);
  data PT_LOAD FLAGS(6);
  interp PT_INTERP;
  dynamic PT_DYNAMIC FLAGS(6);
}

SECTIONS
{
  /* Position independent: the kernel chooses the load bias. */
  . = 0x0;
  .text : {
    KEEP(*(.text.entry))
    *(.text*)
  } :text
  .rodata : { *(.rodata*) } :text
  .rela.dyn : { *(.rela.dyn) } :text
  .interp : { *(.interp) } :text :interp

  . = ALIGN(0x1000);
  .data : { *(.data*) } :data
  .got : { *(.got*) } :data
  .init_array : {
    __init_array_start = .;
    KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
    KEEP(*(.init_array))
    __init_array_end = .;
  } :data
  .dynamic : { *(.dynamic) } :data :dynamic
  .bss : { *(.bss*) *(COMMON) } :data
}
