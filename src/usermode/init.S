    .section .text
    .globl _start

_start:
    /* Set up flat data segments */
    mov $0x43, %rax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    /* Write greeting to stdout */
    mov $1, %rax              # SYS_write
    mov $1, %rdi              # stdout
    lea init_banner(%rip), %rsi
    mov $(init_banner_end - init_banner), %rdx
    int $0x80

    /* loop sleeping so init stays alive */
1:
    mov $35, %rax             # SYS_sleep (microseconds)
    mov $500000, %rdi         # 500ms
    int $0x80
    jmp 1b

    .section .rodata
init_banner:
    .ascii "meniOS v0.0.4 - init process started (PID 1)\n"
    .ascii "System initialization complete. Ready.\n"
init_banner_end:
