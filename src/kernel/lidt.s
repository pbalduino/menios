global idt_load
global idt_generic_isr_asm_handler
global idt_df_isr_asm_handler
global idt_gpf_isr_asm_handler
global idt_pf_isr_asm_handler

extern idt_generic_isr_handler
extern idt_df_isr_handler
extern idt_gpf_isr_handler
extern idt_pf_isr_handler

idt_load:
  lidt [rdi]   ; Load the IDT from the memory location pointed to by rdi
  ret

; This function is the ISR for the "Division by Zero" exception.
; It is called when a division by zero occurs.
idt_generic_isr_asm_handler:
  pushfq                     ; Push the flags register (RFLAGS)
  push rax                   ; Push the accumulator register (RAX)
  push rcx                   ; Push a general-purpose register (RCX) for backup

  ; Optionally, you can save more registers and perform additional error handling here.

  ; Call your C exception handler (division_by_zero_handler) in C code
  call idt_generic_isr_handler

  ; Optionally, you can perform additional error handling after the C handler.
  pop rcx                    ; Restore the backup of RCX
  pop rax                    ; Restore RAX
  popfq                      ; Restore RFLAGS

  iretq                      ; Return from the interrupt

idt_df_isr_asm_handler:
  pushfq                     ; Push the flags register (RFLAGS)
  push rax                   ; Push the accumulator register (RAX)
  push rcx                   ; Push a general-purpose register (RCX) for backup

  ; Optionally, you can save more registers and perform additional error handling here.

  ; Call your C exception handler (division_by_zero_handler) in C code
  call idt_df_isr_handler

  ; Optionally, you can perform additional error handling after the C handler.
  pop rcx                    ; Restore the backup of RCX
  pop rax                    ; Restore RAX
  popfq                      ; Restore RFLAGS

  iretq                      ; Return from the interrupt

idt_gpf_isr_asm_handler:
  pushfq                     ; Push the flags register (RFLAGS)
  push rax                   ; Push the accumulator register (RAX)
  push rcx                   ; Push a general-purpose register (RCX) for backup

  ; Optionally, you can save more registers and perform additional error handling here.

  ; Call your C exception handler (division_by_zero_handler) in C code
  call idt_gpf_isr_handler

  ; Optionally, you can perform additional error handling after the C handler.
  pop rcx                    ; Restore the backup of RCX
  pop rax                    ; Restore RAX
  popfq                      ; Restore RFLAGS

  iretq                      ; Return from the interrupt

idt_pf_isr_asm_handler:
  pushfq                     ; Push the flags register (RFLAGS)
  push rax                   ; Push the accumulator register (RAX)
  push rcx                   ; Push a general-purpose register (RCX) for backup

  ; Optionally, you can save more registers and perform additional error handling here.

  ; Call your C exception handler (division_by_zero_handler) in C code
  call idt_pf_isr_handler

  ; Optionally, you can perform additional error handling after the C handler.
  pop rcx                    ; Restore the backup of RCX
  pop rax                    ; Restore RAX
  popfq                      ; Restore RFLAGS

  iretq                      ; Return from the interrupt
