global idt_load
global idt_generic_isr_asm_handler
global idt_df_isr_asm_handler
global idt_gpf_isr_asm_handler
global idt_pf_isr_asm_handler
global idt_period_timer_isr_asm_handler
global ps2kb_isr_handler
global ahci_isr_handler
global syscall_isr_handler
global syscall_entry

extern idt_generic_isr_handler
extern idt_df_isr_handler
extern idt_gpf_isr_handler
extern idt_pf_isr_handler
extern timer_handler
extern ps2kb_handler
extern ahci_irq_handler
extern syscall_dispatch
extern serial_printf
extern syscall_last_return_value
extern syscall_last_return_slot_value

%define SYSCALL_CONTEXT_KERNEL_RSP 0
%define SYSCALL_CONTEXT_USER_RSP   8
%define SYSCALL_CONTEXT_USER_RIP   16
%define SYSCALL_CONTEXT_USER_RFLAGS 24

%define SYSCALL_FRAME_R11   32
%define SYSCALL_FRAME_RCX   96
%define SYSCALL_FRAME_RIP   120
%define SYSCALL_FRAME_CS    128
%define SYSCALL_FRAME_RFLAGS 136
%define SYSCALL_FRAME_RSP   144
%define SYSCALL_FRAME_SS    152

%define USER_CODE_SELECTOR  0x3B
%define USER_DATA_SELECTOR  0x43

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
  pushfq                     ; Save RFLAGS
  push rax                   ; Save RAX
  push rcx                   ; Save RCX
  push rbx                   ; Save RBX
  push rdx                   ; Save RDX
  push rsi                   ; Save RSI
  push rdi                   ; Save RDI
  push rbp                   ; Save RBP
  push r8                    ; Save R8
  push r9                    ; Save R9
  push r10                   ; Save R10
  push r11                   ; Save R11
  push r12                   ; Save R12
  push r13                   ; Save R13
  push r14                   ; Save R14
  push r15                   ; Save R15

  lea rdi, [rsp]
  call idt_gpf_isr_handler

  pop r15                    ; Restore R15
  pop r14                    ; Restore R14
  pop r13                    ; Restore R13
  pop r12                    ; Restore R12
  pop r11                    ; Restore R11
  pop r10                    ; Restore R10
  pop r9                     ; Restore R9
  pop r8                     ; Restore R8
  pop rbp                    ; Restore RBP
  pop rdi                    ; Restore RDI
  pop rsi                    ; Restore RSI
  pop rdx                    ; Restore RDX
  pop rbx                    ; Restore RBX
  pop rcx                    ; Restore RCX
  pop rax                    ; Restore RAX
  popfq                      ; Restore RFLAGS

  iretq                      ; Return from the interrupt

idt_pf_isr_asm_handler:
  pushfq
  push rax
  push rcx
  push rbx
  push rdx
  push rsi
  push rdi
  push rbp
  push r8
  push r9
  push r10
  push r11
  push r12
  push r13
  push r14
  push r15

  lea rdi, [rsp]
  call idt_pf_isr_handler

  pop r15
  pop r14
  pop r13
  pop r12
  pop r11
  pop r10
  pop r9
  pop r8
  pop rbp
  pop rdi
  pop rsi
  pop rdx
  pop rbx
  pop rcx
  pop rax
  popfq
  add rsp, 8

  iretq

idt_period_timer_isr_asm_handler:
  push rax
  push rbx
  push rcx
  push rdx
  push rbp
  push rsi
  push rdi
  push r8
  push r9
  push r10
  push r11
  push r12
  push r13
  push r14
  push r15

  cld

  lea rdi, [rsp]
  call timer_handler

  pop r15
  pop r14
  pop r13
  pop r12
  pop r11
  pop r10
  pop r9
  pop r8
  pop rdi
  pop rsi
  pop rbp
  pop rdx
  pop rcx
  pop rbx
  pop rax

  ; Return from interrupt
  iretq

ps2kb_isr_handler:
  pushfq                     ; Push the flags register (RFLAGS)
  push rax                   ; Push the accumulator register (RAX)
  push rcx                   ; Push a general-purpose register (RCX) for backup

  ; sub rsp, 8                 ; Align stack to 16 bytes
  call ps2kb_handler

  ; add rsp, 8                 ; Restore stack alignment
  pop rcx                    ; Restore the backup of RCX
  pop rax                    ; Restore RAX
  popfq                      ; Restore RFLAGS

  iretq                      ; Return from the interrupt

ahci_isr_handler:
  push rax
  push rbx
  push rcx
  push rdx
  push rbp
  push rsi
  push rdi
  push r8
  push r9
  push r10
  push r11
  push r12
  push r13
  push r14
  push r15

  cld

  call ahci_irq_handler

  pop r15
  pop r14
  pop r13
  pop r12
  pop r11
  pop r10
  pop r9
  pop r8
  pop rdi
  pop rsi
  pop rbp
  pop rdx
  pop rcx
  pop rbx
  pop rax

  iretq

syscall_isr_handler:
  cld

  push rax
  push rbx
  push rcx
  push rdx
  push rbp
  push rsi
  push rdi
  push r8
  push r9
  push r10
  push r11
  push r12
  push r13
  push r14
  push r15

  mov rdi, rsp
  call syscall_dispatch

  ; Update the saved rax slot with the syscall return value before restoring registers.
  mov [rsp + 14 * 8], rax

  pop r15
  pop r14
  pop r13
  pop r12
  pop r11
  pop r10
  pop r9
  pop r8
  pop rdi
  pop rsi
  pop rbp
  pop rdx
  pop rcx
  pop rbx
  pop rax

  iretq

syscall_entry:
  swapgs

  mov [gs:SYSCALL_CONTEXT_USER_RSP], rsp
  mov [gs:SYSCALL_CONTEXT_USER_RIP], rcx
  mov [gs:SYSCALL_CONTEXT_USER_RFLAGS], r11
  mov rsp, [gs:SYSCALL_CONTEXT_KERNEL_RSP]
  test rsp, rsp
  jnz .syscall_stack_ready
  hlt
.syscall_stack_ready:
  and rsp, 0xfffffffffffffff0

  push qword USER_DATA_SELECTOR
  push qword [gs:SYSCALL_CONTEXT_USER_RSP]
  push r11
  push qword USER_CODE_SELECTOR
  push rcx

  push rax
  push rbx
  push rcx
  push rdx
  push rbp
  push rsi
  push rdi
  push r8
  push r9
  push r10
  push r11
  push r12
  push r13
  push r14
  push r15

  mov rdi, rsp
  call syscall_dispatch

  mov [rsp + 14 * 8], rax
  mov [rel syscall_last_return_value], rax
  mov rdx, [rsp + 14 * 8]
  mov [rel syscall_last_return_slot_value], rdx
  mov rdi, sysret_value_fmt
  mov rsi, rax
  mov rdx, [rsp + 14 * 8]
  call serial_printf
  mov rcx, [rsp + 13 * 8]
  mov rbx, [rsp + 12 * 8]
  mov [rel syscall_last_return_slot_value], rcx
  mov [rel syscall_last_return_value], rbx

  pop r15
  pop r14
  pop r13
  pop r12
  pop r11
  pop r10
  pop r9
  pop r8
  pop rdi
  pop rsi
  pop rbp
  pop rdx
  pop rcx
  pop rbx
  pop rax

  swapgs
  iretq

section .rodata
sysret_value_fmt:
  db "[sysret] value=%lx slot=%lx", 10, 0
