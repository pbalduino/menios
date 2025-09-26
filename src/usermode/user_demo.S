    .section .text
    .globl _start

_start:
    mov $1, %rax            # SYS_write
    mov $1, %rdi            # stdout
    lea message(%rip), %rsi
    mov $message_end - message, %rdx
    int $0x80

    mov $60, %rax           # SYS_exit
    xor %rdi, %rdi
    int $0x80

    .section .rodata
message:
    .ascii "Hello from user ELF via int 0x80!\n"
message_end:
    .byte 0
