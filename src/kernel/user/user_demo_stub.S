    .section .rodata
    .global user_demo_stub_start
    .global user_demo_stub_end

user_demo_stub_start:
    mov $1, %rax
    mov $1, %rdi
    lea user_demo_message(%rip), %rsi
    mov $user_demo_message_end - user_demo_message, %rdx
    int $0x80

    mov $60, %rax
    xor %rdi, %rdi
    int $0x80

user_demo_message:
    .ascii "Hello from Ring 3 via int 0x80!\n"
user_demo_message_end:
    .byte 0

user_demo_stub_end:
