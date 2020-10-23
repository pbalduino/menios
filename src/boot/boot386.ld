ENTRY(boot)
OUTPUT_FORMAT("binary")
SECTIONS {
  . = 0x7c00;
  .text :
  {
    *(.boot)
    *(.text)
  }
  .rodata :
  {
    *(.rodata)
  }
  .data :
  {
    . = ALIGN(0x1000);
    __START_DATA__ = .;
    *(.data)
    __END_DATA__ = .;
  }
  .bss :
  {
    . = ALIGN(0x1000);
    __START_BSS__ = .;
    *(.bss)
    __END_BSS__ = .;
  }
  PROVIDE(_MEM_END_ = .);
}
