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

    # Select prefix based on argument in RDI (0..2).
    mov %rdi, %rax
    cmp $2, %rax
    jbe 1f
    xor %rax, %rax
1:
    lea prefixes(%rip), %rbx
    mov %rax, %r9
    shl $4, %r9
    mov (%rbx,%r9,1), %rsi
    mov 8(%rbx,%r9,1), %rdx
    mov $1, %rax            # SYS_write
    mov $1, %rdi            # stdout
    int $0x80

    mov $5, %rcx            # outer iterations
outer_loop:
    lea messages(%rip), %rbx
    mov $3, %r8             # messages per iteration
print_loop:
    mov (%rbx), %rsi
    mov 8(%rbx), %rdx
    mov $1, %rax            # SYS_write
    mov $1, %rdi            # stdout
    int $0x80

    add $16, %rbx
    dec %r8
    jnz print_loop

    mov $200000, %rdi       # sleep for 200,000 microseconds (200 ms)
    mov $35, %rax           # SYS_sleep
    int $0x80

    mov $24, %rax           # SYS_yield
    int $0x80

    loop outer_loop

    mov $1, %rax            # final message
    mov $1, %rdi
    lea msg_done(%rip), %rsi
    mov $(msg_done_end - msg_done), %rdx
    int $0x80

    mov $60, %rax           # SYS_exit
    xor %rdi, %rdi
    int $0x80

    .section .rodata
prefixes:
    .quad prefix0
    .quad prefix0_end - prefix0
    .quad prefix1
    .quad prefix1_end - prefix1
    .quad prefix2
    .quad prefix2_end - prefix2

messages:
    .quad msg_tick
    .quad msg_tick_end - msg_tick
    .quad msg_sleep
    .quad msg_sleep_end - msg_sleep
    .quad msg_yield
    .quad msg_yield_end - msg_yield

prefix0:
    .ascii "[user_demo low] starting scheduler demo\n"
prefix0_end:

prefix1:
    .ascii "[user_demo normal] ready to run\n"
prefix1_end:

prefix2:
    .ascii "[user_demo high] entering main loop\n"
prefix2_end:

msg_tick:
    .ascii "[user_demo] tick...\n"
msg_tick_end:

msg_sleep:
    .ascii "[user_demo] sleeping for 200ms\n"
msg_sleep_end:

msg_yield:
    .ascii "[user_demo] yielding CPU\n"
msg_yield_end:

msg_done:
    .ascii "[user_demo] completed demo loop\n"
msg_done_end:
