# Simple standalone "Hello, world" for meniOS.
#
# Assemble inside meniOS with:
#   as --64 -o hello.o hello.s
#   ld -o hello hello.o
# Then run ./hello

	.section .text
	.global _start

_start:
	mov	$1, %rax              # SYS_write
	mov	$1, %rdi              # stdout
	lea	message(%rip), %rsi
	mov	$message_len, %rdx
	syscall

	mov	$60, %rax             # SYS_exit
	xor	%rdi, %rdi
	syscall

	.section .rodata
message:
	.ascii	"Hello from hello.s on meniOS!\n"
message_len = . - message
