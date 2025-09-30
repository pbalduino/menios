    .section .text
    .globl _start

.equ SYS_WRITE,    1
.equ SYS_WAITPID, 67
.equ SYS_SLEEP,   35
.equ SYS_YIELD,   24

_start:
    mov $0x43, %rax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    lea init_banner(%rip), %rsi
    mov $(init_banner_end - init_banner), %rdx
    mov $SYS_WRITE, %rax
    mov $1, %rdi
    int $0x80

wait_loop:
    mov $SYS_WAITPID, %rax
    mov $-1, %rdi
    lea wait_status(%rip), %rsi
    xor %rdx, %rdx
    int $0x80
    test %rax, %rax
    jl no_child

    lea child_reaped_msg(%rip), %rsi
    mov $(child_reaped_msg_end - child_reaped_msg), %rdx
    mov $SYS_WRITE, %rax
    mov $1, %rdi
    int $0x80

    mov $SYS_YIELD, %rax
    int $0x80
    jmp wait_loop

no_child:
    mov $SYS_SLEEP, %rax
    mov $1000000, %rdi        # 1 second
    int $0x80
    mov $SYS_YIELD, %rax
    int $0x80
    jmp wait_loop

    .section .data
init_banner:
    .ascii "[init] pid 1 supervisor online\n"
init_banner_end:

child_reaped_msg:
    .ascii "[init] child reaped\n"
child_reaped_msg_end:

    .section .bss
    .balign 8
wait_status:
    .space 4
