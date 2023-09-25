global idt_load
global idt_division_by_zero_isr

extern idt_division_by_zero_handler

idt_load:
    lidt [rdi]   ; Load the IDT from the memory location pointed to by rdi
    ret

; This function is the ISR for the "Division by Zero" exception.
; It is called when a division by zero occurs.
idt_division_by_zero_isr:
    pushfq                     ; Push the flags register (RFLAGS)
    push rax                   ; Push the accumulator register (RAX)
    push rcx                   ; Push a general-purpose register (RCX) for backup

    ; Optionally, you can save more registers and perform additional error handling here.

    ; Call your C exception handler (division_by_zero_handler) in C code
    call idt_division_by_zero_handler

    ; Optionally, you can perform additional error handling after the C handler.
    pop rcx                    ; Restore the backup of RCX
    pop rax                    ; Restore RAX
    popfq                      ; Restore RFLAGS
    iretq                      ; Return from the interrupt
