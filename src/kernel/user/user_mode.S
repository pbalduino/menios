    .text
    .globl user_mode_enter

    .equ USER_CODE_SELECTOR, 0x3B
    .equ USER_DATA_SELECTOR, 0x43

user_mode_enter:
    /* rdi = entrypoint, rsi = user stack top, rdx = argument */
    cld

    mov %rdi, %r8
    mov %rdx, %rdi

    mov %rsi, %rax
    and $~0xF, %rax
    lea -8(%rax), %rax

    mov $USER_DATA_SELECTOR, %cx
    mov %cx, %ds
    mov %cx, %es
    mov %cx, %fs
    mov %cx, %gs

    pushq $USER_DATA_SELECTOR
    pushq %rax

    pushfq
    pop %rcx
    or $0x200, %rcx
    push %rcx

    pushq $USER_CODE_SELECTOR
    pushq %r8
    iretq
