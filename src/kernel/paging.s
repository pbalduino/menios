bits 32

extern page_fault_handler

global init_paging
global isr_page_fault

init_paging:
  push ebp
  mov ebp, esp
  mov eax, [ebp + 8]
  mov cr3, eax

  mov eax, cr0
  or eax, 0x80000001
  mov cr0, eax

  pop ebp
  ret

isr_page_fault:
  pusha
  call page_fault_handler
  popa
  ; mov eax, 0xbadc0de
  iret
