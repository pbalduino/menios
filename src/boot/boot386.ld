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
    *(.data)
  }
  .bss : ALIGN(0x1000)
  {
    . = 0xf000;
    _BSS_START_ = .;
    *(.bss)
    _BSS_END_ = .;
  }
  PROVIDE(_MEM_END_ = .);
}
