    .text
    .globl proc_enter_userspace
    .type proc_enter_userspace, @function
proc_enter_userspace:
    mov %rdi, %rsp
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rdi
    pop %rsi
    pop %rbp
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax
    iretq
