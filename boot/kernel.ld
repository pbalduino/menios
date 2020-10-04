ENTRY(boot)
OUTPUT_FORMAT("binary")
SECTIONS {
    . = 0x7c00;
    .text :
    {
        *(.boot)
        *(.text)
    }
    .rodata ALIGN (4K) :
    {
        *(.rodata)
    }
    .data ALIGN (4K) :
    {
        *(.data)
    }
    .bss ALIGN (4K) :
    {
        *(.bss)
    }
    PROVIDE(end = .);
}
