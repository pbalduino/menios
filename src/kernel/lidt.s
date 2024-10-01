.type idt_load, @function
.global idt_load

.type idt_generic_isr_asm_handler, @function
.global idt_generic_isr_asm_handler

.type idt_df_isr_asm_handler, @function
.global idt_df_isr_asm_handler

.global idt_gpf_isr_asm_handler
.global idt_pf_isr_asm_handler

.extern idt_generic_isr_handler
.extern idt_df_isr_handler
.extern idt_gpf_isr_handler
.extern page_fault_handler

idt_load:
  lidt (%rdi)   // Load the IDT from the memory location pointed to by %rdi
  ret

// This function is the ISR for the "Division by Zero" exception.
// It is called when a division by zero occurs.
idt_generic_isr_asm_handler:
  pushfq                     // Push the flags register (RFLAGS)
  push %rax                   // Push the accumulator register (RAX)
  push %rcx                   // Push a general-purpose register (RCX) for backup

  // Optionally, you can save more registers and perform additional error handling here.

  // Call your C exception handler (division_by_zero_handler) in C code
  call idt_generic_isr_handler

  // Optionally, you can perform additional error handling after the C handler.
  pop %rcx                    // Restore the backup of %rcx
  pop %rax                    // Restore %rax
  popfq                      // Restore RFLAGS

  iretq                      // Return from the interrupt

idt_df_isr_asm_handler:
  pushfq                     // Push the flags register (RFLAGS)
  push %rax                   // Push the accumulator register (RAX)
  push %rcx                   // Push a general-purpose register (RCX) for backup

  // Optionally, you can save more registers and perform additional error handling here.

  // Call your C exception handler (division_by_zero_handler) in C code
  call idt_df_isr_handler

  // Optionally, you can perform additional error handling after the C handler.
  pop %rcx                    // Restore the backup of %rcx
  pop %rax                    // Restore %rax
  popfq                      // Restore RFLAGS

  iretq                      // Return from the interrupt

idt_gpf_isr_asm_handler:
  pushfq                     // Save RFLAGS
  push %rax                   // Save %rax
  push %rcx                   // Save %rcx
  push %rbx                   // Save %rbx
  push %rdx                   // Save %rdx
  push %rsi                   // Save %rsi
  push %rdi                   // Save %rdi
  push %rbp                   // Save RBP
  push %r8                    // Save R8
  push %r9                    // Save R9
  push %r10                   // Save R10
  push %r11                   // Save R11
  push %r12                   // Save R12
  push %r13                   // Save R13
  push %r14                   // Save R14
  push %r15                   // Save R15

  // Call your C exception handler (division_by_zero_handler) in C code
  call idt_gpf_isr_handler

  // Restore registers in reverse order
  pop %r15                    // Restore R15
  pop %r14                    // Restore R14
  pop %r13                    // Restore R13
  pop %r12                    // Restore R12
  pop %r11                    // Restore R11
  pop %r10                    // Restore R10
  pop %r9                     // Restore R9
  pop %r8                     // Restore R8
  pop %rbp                    // Restore RBP
  pop %rdi                    // Restore %rdi
  pop %rsi                    // Restore %rsi
  pop %rdx                    // Restore %rdx
  pop %rbx                    // Restore %rbx
  pop %rcx                    // Restore %rcx
  pop %rax                    // Restore %rax
  popfq                      // Restore RFLAGS

  iretq                      // Return from the interrupt

idt_pf_isr_asm_handler:
  // Push the stack segment (SS)
  push (%rsp)

  // Push the instruction pointer (RIP), code segment (CS), RFLAGS, stack pointer (RSP), and stack segment (SS)
  push 8(%rsp)  // RIP (was automatically saved by the CPU)
  push 16(%rsp) // CS (was automatically saved by the CPU)
  push 24(%rsp) // RFLAGS (was automatically saved by the CPU)
  push 32(%rsp) // RSP (was automatically saved by the CPU)
  push 40(%rsp) // SS (was automatically saved by the CPU)

  // Call the C handler
  call page_fault_handler
  
  // Clean up the stack (6 pushes * 8 bytes each = 48 bytes)
  addq $48, %rsp
  
  // Return from interrupt
  iretq
  