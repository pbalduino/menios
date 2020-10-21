bits 32

extern keyboard_handler
extern timer_handler

global irq_keyboard
global irq_timer
global reset_timer

section .text

irq_keyboard:
  cli
	pusha
	call keyboard_handler
	popa
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
