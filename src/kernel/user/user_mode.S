    .section .text
    .globl user_mode_enter

/* void user_mode_enter(void (*entrypoint)(void*), void* user_stack_top, uint64_t arg) */
user_mode_enter:
    cld

    mov %rdi, %r8           /* preserve entry point */
    mov %rdx, %rdi          /* first argument */

    mov %rsi, %rax
    and $-16, %rax          /* align stack */
    lea -8(%rax), %rax      /* reserve return slot */

    mov $0x43, %cx          /* USER_DATA_SELECTOR */
    mov %cx, %ds
    mov %cx, %es
    mov %cx, %fs
    mov %cx, %gs

    pushq $0x43
    pushq %rax

    pushfq
    pop %rcx
    or $0x200, %rcx         /* enable IF */
    pushq %rcx

    pushq $0x3B             /* USER_CODE_SELECTOR */
    pushq %r8
    iretq
