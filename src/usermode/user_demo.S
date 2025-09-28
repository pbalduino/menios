    .section .text
    .globl _start

_start:
    mov $0x43, %rax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    sub $8192, %rsp         # touch a second stack page (lazy allocation)
    movb $'A', (%rsp)
    add $8192, %rsp

    lea messages(%rip), %rbx
    mov $3, %rcx

loop_messages:
    mov (%rbx), %rsi        # message pointer
    mov 8(%rbx), %rdx       # message length
    mov $1, %rax            # SYS_write
    mov $1, %rdi            # stdout
    int $0x80

    add $16, %rbx
    loop loop_messages

    mov $60, %rax           # SYS_exit
    mov $42, %rdi           # exit code
    int $0x80

    .section .rodata
messages:
    .quad msg_hello
    .quad msg_hello_end - msg_hello
    .quad msg_stack
    .quad msg_stack_end - msg_stack
    .quad msg_exit
    .quad msg_exit_end - msg_exit

msg_hello:
    .ascii "[user_demo] Hello from user mode via int 0x80!\n"
msg_hello_end:

msg_stack:
    .ascii "[user_demo] Forced stack touch across 8KiB boundary.\n"
msg_stack_end:

msg_exit:
    .ascii "[user_demo] Exiting with code 42.\n"
msg_exit_end:
