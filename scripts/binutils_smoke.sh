#!/bin/sh
set -eu

# simple assembly program: return 0 from _start
cat > hello.s <<'SRC'
        .section .text
        .globl _start
_start:
        mov $1, %rax      # SYS_write
        mov $1, %rdi      # STDOUT
        lea msg(%rip), %rsi
        mov $msg_len, %rdx
        syscall
        mov $60, %rax     # SYS_exit
        xor %rdi, %rdi
        syscall

        .section .rodata
msg:
        .ascii "Hello from meniOS!\n"
msg_len = . - msg
SRC

echo "[binutils] assembling hello.s"
as --64 -o hello.o hello.s

ls -l hello.o

nm hello.o | head -n 5
objdump -d hello.o

echo "[binutils] linking hello"
ld -o hello hello.o

objdump -d hello | head -n 20

rm -f hello.s hello.o hello
