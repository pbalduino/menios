ENTRY(_start)
SECTIONS
{
  . = 0x0000000000400000;

  .text : {
    *(.text*)
  }

  .rodata : {
    *(.rodata*)
    *(.rodata.str*)
  }

  .data : {
    *(.data*)
  }

  .bss : {
    *(.bss*)
    *(COMMON)
  }
}
