    .section .text
    .globl _start
    .type _start, @function

    .extern main
    .extern exit
    .extern __menios_init_libc
    .extern __menios_fini_libc

_start:
    xor    %ebp, %ebp

    // Preserve argc/argv/envp across library initialization.
    push   %rdx
    push   %rsi
    push   %rdi
    call   __menios_init_libc
    pop    %rdi
    pop    %rsi
    pop    %rdx

    // Align the stack for the call into main.
    and    $-16, %rsp
    call   main

    // Capture the return value before finalizers run.
    mov    %eax, %edi
    push   %rdi
    call   __menios_fini_libc
    pop    %rdi

    call   exit

1:
    hlt
    jmp 1b

    .size _start, . - _start
