; reference: https://en.wikipedia.org/wiki/Control_register

section .text
global enable_paging
global load_page_directory

load_page_directory:
  push ebp
  mov ebp, esp
  mov eax, [esp + 8]           ; load unsigned int* parameter
  mov cr3, eax                 ; point CR3 to parameter address
  mov esp, ebp
  pop ebp
  ret

enable_paging:
  push ebp
  mov ebp, esp
  mov eax, cr0                 ; copy CR0 value
  or eax, 0x80000001           ; enables bit 31 - memory paging
  mov cr0, eax                 ; update CR0 value
  mov esp, ebp
  pop ebp
  ret
