    .section .text
.globl _start

.equ SYS_FB_GETINFO, 200
.equ SYS_FB_MAP,     201
.equ SYS_FB_FLIP,    202

_start:
    mov $0x43, %rax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    mov %rdi, %r13          # preserve initial prefix selector

    sub $8192, %rsp         # touch a second stack page (lazy allocation)
    movb $'A', (%rsp)
    add $8192, %rsp

    lea pipe_fds(%rip), %rdi
    mov $22, %rax           # SYS_pipe
    int $0x80

    mov $57, %rax           # SYS_fork
    int $0x80
    test %rax, %rax
    jne 1f

    mov $3, %rax            # close write end in child
    mov pipe_fds+4(%rip), %edi
    int $0x80

    mov $0, %rax            # read from pipe
    mov pipe_fds(%rip), %edi
    lea pipe_buffer(%rip), %rsi
    mov $64, %rdx
    int $0x80
    mov %rax, %r12          # store number of bytes read

    lea msg_fork_child(%rip), %rsi
    mov $(msg_fork_child_end - msg_fork_child), %rdx
    mov $1, %rax
    mov $2, %rdi
    int $0x80

    mov $1, %rax            # emit pipe payload prefix
    mov $1, %rdi
    lea msg_pipe_child(%rip), %rsi
    mov $(msg_pipe_child_end - msg_pipe_child), %rdx
    int $0x80

    mov $1, %rax            # write buffered payload
    mov $1, %rdi
    lea pipe_buffer(%rip), %rsi
    mov %r12, %rdx
    int $0x80

    mov $60, %rax           # child exits immediately after reporting
    xor %rdi, %rdi
    int $0x80

1:
    mov $3, %rax            # close read end in parent
    mov pipe_fds(%rip), %edi
    int $0x80

    lea msg_pipe_payload(%rip), %rsi
    mov $(msg_pipe_payload_end - msg_pipe_payload), %rdx
    mov $1, %rax
    mov pipe_fds+4(%rip), %edi
    int $0x80

    mov $3, %rax            # close write end in parent
    mov pipe_fds+4(%rip), %edi
    int $0x80

    lea msg_fork_parent(%rip), %rsi
    mov $(msg_fork_parent_end - msg_fork_parent), %rdx
    mov $1, %rax
    mov $2, %rdi
    int $0x80

    lea msg_input_intro(%rip), %rsi
    mov $(msg_input_intro_end - msg_input_intro), %rdx
    mov $1, %rax
    mov $2, %rdi
    int $0x80

    mov $2, %rax            # SYS_open
    lea input_device_path(%rip), %rdi
    xor %rsi, %rsi          # O_RDONLY
    xor %rdx, %rdx
    int $0x80
    mov %rax, %r15          # store kbd fd
    cmp $0, %rax
    jge input_open_ok

    lea msg_input_open_fail(%rip), %rsi
    mov $(msg_input_open_fail_end - msg_input_open_fail), %rdx
    mov $1, %rax
    mov $1, %rdi
    int $0x80
    jmp input_demo_done

input_open_ok:
    mov $3, %r10d           # collect three events
    lea hex_digits(%rip), %r12

input_read_loop:
    mov $0, %rax            # SYS_read
    mov %r15d, %edi
    lea input_event_buffer(%rip), %rsi
    mov $16, %rdx
    int $0x80
    cmp $16, %rax
    jne input_close

    movzwl input_event_buffer+8(%rip), %eax
    mov %eax, %edx
    shr $12, %edx
    mov (%r12,%rdx,1), %bl
    mov %bl, kbd_event_scancode(%rip)
    movzwl input_event_buffer+8(%rip), %eax
    shr $8, %eax
    and $0xF, %eax
    mov (%r12,%rax,1), %bl
    mov %bl, kbd_event_scancode+1(%rip)
    movzwl input_event_buffer+8(%rip), %eax
    shr $4, %eax
    and $0xF, %eax
    mov (%r12,%rax,1), %bl
    mov %bl, kbd_event_scancode+2(%rip)
    movzwl input_event_buffer+8(%rip), %eax
    and $0xF, %eax
    mov (%r12,%rax,1), %bl
    mov %bl, kbd_event_scancode+3(%rip)

    movzbq input_event_buffer+10(%rip), %rax
    mov %rax, %rdx
    shr $4, %rdx
    mov (%r12,%rdx,1), %bl
    mov %bl, kbd_event_ascii(%rip)
    movzbq input_event_buffer+10(%rip), %rax
    and $0xF, %rax
    mov (%r12,%rax,1), %bl
    mov %bl, kbd_event_ascii+1(%rip)
    movzbq input_event_buffer+10(%rip), %rax
    mov %al, %bl
    cmp $32, %al
    jb input_ascii_not_printable
    cmp $126, %al
    ja input_ascii_not_printable
    jmp input_ascii_store

input_ascii_not_printable:
    mov $'.', %bl

input_ascii_store:
    mov %bl, kbd_event_char(%rip)

    movzbq input_event_buffer+11(%rip), %rax
    mov %rax, %rdx
    shr $4, %rdx
    mov (%r12,%rdx,1), %bl
    mov %bl, kbd_event_mods(%rip)
    movzbq input_event_buffer+11(%rip), %rax
    and $0xF, %rax
    mov (%r12,%rax,1), %bl
    mov %bl, kbd_event_mods+1(%rip)

    movzbq input_event_buffer+12(%rip), %rax
    test %al, %al
    jz input_pressed_up
    mov $'D', %bl
    jmp input_pressed_store

input_pressed_up:
    mov $'U', %bl

input_pressed_store:
    mov %bl, kbd_event_pressed(%rip)

    mov $1, %rax
    mov $1, %rdi
    lea kbd_event_msg(%rip), %rsi
    mov $(kbd_event_msg_end - kbd_event_msg), %rdx
    int $0x80

    dec %r10d
    jz input_close
    jmp input_read_loop

input_close:
    mov $3, %rax            # SYS_close
    mov %r15d, %edi
    int $0x80

input_demo_done:

    call framebuffer_demo

    mov %r13, %r14          # select prefix based on original argument (0..2)

2:
    mov %r14, %rax
    cmp $2, %rax
    jbe 3f
    xor %rax, %rax
3:
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

framebuffer_demo:
    push %rbx
    push %rbp
    push %r12
    push %r13
    push %r14
    push %r15

    mov $SYS_FB_GETINFO, %rax
    lea fb_info(%rip), %rdi
    int $0x80
    cmp $0, %rax
    jl fb_demo_cleanup

    mov $SYS_FB_MAP, %rax
    int $0x80
    cmp $0, %rax
    jl fb_demo_cleanup
    mov %rax, %r12
    mov %rax, fb_buffer_ptr(%rip)

    mov fb_info+12(%rip), %eax   # bpp
    cmp $32, %eax
    jne fb_demo_present

    mov $1, %rax
    mov $1, %rdi
    lea msg_fb_demo(%rip), %rsi
    mov $(msg_fb_demo_end - msg_fb_demo), %rdx
    int $0x80

    mov fb_info+4(%rip), %r14d   # height
    mov fb_info(%rip), %r15d     # width
    mov fb_info+8(%rip), %ebx    # pitch
    xor %r10d, %r10d             # row index

fb_row_loop:
    cmp %r14d, %r10d
    jle fb_demo_present

    mov %r10d, %eax
    imul %ebx, %eax
    mov %eax, %r11d
    mov %r12, %r13
    add %r11, %r13

    xor %r9d, %r9d               # column index
fb_col_loop:
    cmp %r15d, %r9d
    jle fb_row_next

    mov %r9d, %edx
    shl $2, %edx

    mov %r10d, %ecx
    shl $16, %ecx
    mov %r9d, %eax
    shl $8, %eax
    or %ecx, %eax
    or $0x000000FF, %eax
    mov %eax, (%r13,%rdx)

    inc %r9d
    jmp fb_col_loop

fb_row_next:
    inc %r10d
    jmp fb_row_loop

fb_demo_present:
    mov $SYS_FB_FLIP, %rax
    mov fb_buffer_ptr(%rip), %rdi
    xor %rsi, %rsi
    int $0x80

fb_demo_cleanup:
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbp
    pop %rbx
    ret

    .section .data
input_device_path:
    .ascii "/dev/input/kbd\0"

msg_input_intro:
    .ascii "[input_demo] waiting for keyboard events (3 samples)\n"
msg_input_intro_end:

msg_input_open_fail:
    .ascii "[input_demo] failed to open /dev/input/kbd\n"
msg_input_open_fail_end:

msg_fb_demo:
    .ascii "[user_demo] drawing framebuffer gradient\n"
msg_fb_demo_end:

kbd_event_msg:
    .ascii "kbd event: scancode=0x"
kbd_event_scancode:
    .ascii "0000"
kbd_event_mid1:
    .ascii " ascii=0x"
kbd_event_ascii:
    .ascii "00"
kbd_event_mid2:
    .ascii " ('"
kbd_event_char:
    .byte '.'
kbd_event_mid3:
    .ascii "') mods=0x"
kbd_event_mods:
    .ascii "00"
kbd_event_mid4:
    .ascii " pressed="
kbd_event_pressed:
    .byte ' '
kbd_event_endline:
    .ascii "\n"
kbd_event_msg_end:

input_event_buffer:
    .space 16

    .section .rodata
hex_digits:
    .ascii "0123456789ABCDEF"

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

msg_pipe_child:
    .ascii "[pipe child] received: "
msg_pipe_child_end:

msg_pipe_payload:
    .ascii "payload from parent via pipe\n"
msg_pipe_payload_end:

msg_fork_parent:
    .ascii "[user_demo] forked child process\n"
msg_fork_parent_end:

msg_fork_child:
    .ascii "[user_demo] hello from child after fork\n"
msg_fork_child_end:

    .section .bss
    .balign 8
pipe_fds:
    .space 8

    .balign 8
fb_info:
    .space 32

fb_buffer_ptr:
    .quad 0
pipe_buffer:
    .space 64
