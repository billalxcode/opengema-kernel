ENTRY(_start)

PHDRS
{
  image PT_LOAD FLAGS(7);
  interp PT_INTERP;
  dynamic PT_DYNAMIC FLAGS(6);
}

SECTIONS
{
  . = 0x10000000000;
  .text : {
    KEEP(*(.text.entry))
    *(.text*)
  } :image
  .rodata : { *(.rodata*) } :image
  .rela.dyn : { *(.rela.dyn) } :image
  .interp : { *(.interp) } :image :interp
  .data : { *(.data*) } :image
  .dynamic : { *(.dynamic) } :image :dynamic
  .bss : { *(.bss*) *(COMMON) } :image
}
