ENTRY(init_kernel)
OUTPUT_FORMAT("binary")
SECTIONS {
  . = 0x7c00;
  .text :
  {
    *(.boot)
    *(.text)
  }
  .rodata:
  {
    *(.rodata)
  }
  .data:
  {
    *(.data)
  }
  .bss ALIGN (4K) :
  {
    *(.bss)
  }
  PROVIDE(end = .);
}
