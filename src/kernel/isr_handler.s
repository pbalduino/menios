bits 32

extern isr_handler
extern DATA_SEG

%macro ISR_NO_ERRNO 1
global isr%1
isr%1:
  cli                 ; Disable interrupts
  push 0x00           ; Push a dummy error code (if ISR0 doesn't push it's own error code)
  push %1             ; Push the interrupt number (0)
  jmp isr_common_stub ; Go to our common handler.
%endmacro

; The specific CPU interrupts that put an error code
; on the stack are 8, 10, 11, 12, 13, 14 and 17.
; This macro should be used for those interrupts.
%macro ISR_ERRNO 1
global isr%1
isr%1:
  cli                 ; Disable interrupts
  push %1             ; Push the interrupt number (0)
  jmp isr_common_stub ; Go to our common handler.
%endmacro

isr_common_stub:
  pusha               ; Pushes edi,esi,ebp,esp,ebx,edx,ecx,eax

  mov ax, ds          ; Lower 16-bits of eax = ds.
  push eax            ; save the data segment descriptor

  mov ax, DATA_SEG    ; load the kernel data segment descriptor
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax

  call isr_handler

  pop eax             ; reload the original data segment descriptor
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax

  popa                ; Pops edi,esi,ebp...
  add esp, 8          ; Cleans up the pushed error code and pushed ISR number
  mov al, 0x20
  out 0x20, al
  sti
  iret                ; pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP

ISR_ERRNO 8
ISR_ERRNO 14
