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
  call keyboard_handler
  iret

irq_timer:
  cli
  push 0x00
  push 0x20
  call timer_handler
  iret

reset_timer:
  pusha
  mov al, 0x36
  out 0x43, al         ; tell the PIT which channel we're setting
  mov ax, 1193182
  out 0x40, al         ; send low byte
  mov al, ah
  out 0x40, al         ; send high byte")
  popa
  ret

section .data
timer_text: db "Timer! \o/", 13, 10, 0
uhu_text: db "Keyboard! \o/", 13, 10, 0
