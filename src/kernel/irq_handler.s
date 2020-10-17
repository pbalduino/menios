bits 32

extern keyboard_handler
extern timer_handler

global irq_keyboard
global irq_timer
global reset_timer

section .text

irq_keyboard:
  cli
  push 0x00
  push 0x21
	pusha
	push ds
	push es
	push fs
	push gs
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov eax, esp
	push eax
	mov eax, keyboard_handler
	call eax
	pop eax
	pop gs
	pop fs
	pop es
	pop ds
	popa
	add esp, 8
	iret

irq_timer:
  cli
  pusha
  call timer_handler
  popa
  iret

; 11931820 = B610AC
reset_timer:
  pusha
  mov al, 0x36
  out 0x43, al         ; tell the PIT which channel we're setting
  mov ax, 11931
  out 0x40, al         ; send low byte
  mov al, ah
  out 0x40, al         ; send high byte")
  popa
  ret
