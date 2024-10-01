.type loadGDT, @function
.global gdt_load

gdt_load:
  lgdt (%rdi)

  // Set the segment registers to appropriate values for kernel mode
	movw $0x30, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs
	movw %ax, %ss

  movq $0x28, %rax // Kernel code segment
  push %rax
  push %rdi
  lretq

user_mode_init:
  // Set the segment registers to appropriate values for user mode
  movw $0x40, %ax // User data segment
  movw %ax, %ds
  movw %ax, %es
  movw %ax, %fs
  movw %ax, %gs
  movw %ax, %ss

  movq $0x38, %rax // User code segment
  push %rax
  lretq
