#include <kernel/gdt.h>

.global user_mode_enter
.type user_mode_enter, @function
user_mode_enter:
    /* rdi = entrypoint, rsi = user stack top, rdx = argument */
    cld

    mov %rdi, %r8              /* preserve entry pointer */
    mov %rdx, %rdi             /* user function argument in rdi */

    mov %rsi, %rax             /* user stack */
    and $~0xFULL, %rax         /* 16-byte align */
    lea -8(%rax), %rax         /* adjust for ABI (simulate call return) */

    mov $USER_DATA_SEGMENT, %cx
    mov %cx, %ds
    mov %cx, %es
    mov %cx, %fs
    mov %cx, %gs

    pushq $USER_DATA_SEGMENT
    pushq %rax

    pushfq
    pop %rcx
    or $0x200, %rcx            /* enable interrupts */
    push %rcx

    pushq $USER_CODE_SEGMENT
    pushq %r8

    iretq

.size user_mode_enter, .-user_mode_enter
