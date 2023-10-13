section .text
global reset_timer

reset_timer:
  push rax            ; Save the value of RAX register

  mov al, 0x34        ; Move the value 0x34 into the AL register
  out 0x43, al        ; Output the value in AL to the PIT command port (0x43)

  mov ax, 11931       ; Move the value 11931 into the AX register
  out 0x40, al        ; Output the low byte of AX to PIT channel 2 (0x40)

  mov al, ah          ; Move the high byte of AX into AL
  out 0x40, al        ; Output the high byte of AX to PIT channel 2 (0x40)

  pop rax             ; Restore the value of RAX register
  ret                 ; Return from the subroutine
