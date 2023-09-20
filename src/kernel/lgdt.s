global gdt_load

gdt_load:
  lgdt [rdi]

  ; Set the segment registers to appropriate values for kernel mode
  mov ax, 0x30 ; Kernel data segment
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax
  pop rdi

  mov rax, 0x28 ; Kernel code segment
  push rax
  push rdi
  retfq

user_mode_init:
  ; Set the segment registers to appropriate values for user mode
  mov ax, 0x40 ; User data segment
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax

  mov rax, 0x38 ; User code segment
  push rax
  retfq
