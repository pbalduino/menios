section .boot
bits 16

global boot
global idt_load

%macro ISR_NO_ERRNO 1
global isr%1
isr%1:
  cli                 ; Disable interrupts
  push 0x00      ; Push a dummy error code (if ISR0 doesn't push it's own error code)
  push %1        ; Push the interrupt number (0)
  jmp isr_common_stub ; Go to our common handler.
%endmacro

; The specific CPU interrupts that put an error code
; on the stack are 8, 10, 11, 12, 13, 14 and 17.
; This macro should be used for those interrupts.
%macro ISR_ERRNO 1
global isr%1
isr%1:
  cli                 ; Disable interrupts
  push byte %1        ; Push the interrupt number (0)
  jmp isr_common_stub ; Go to our common handler.
%endmacro

; This macro creates a stub for an IRQ - the first parameter is
; the IRQ number, the second is the ISR number it is remapped to.
%macro IRQ 2
global irq%1
irq%1:
  cli
  push  0
  push %2 ; push byte %2
  jmp irq_common_stub
%endmacro

boot:
	mov [disk], dl                ; disk is given by BIOS

	mov ah, 0x02                  ; read sectors
	mov al, 0x10                  ; sectors to read
	mov ch, 0x00                  ; cylinder idx
	mov dh, 0x00                  ; head idx
	mov cl, 0x02                  ; sector idx
	mov dl, [disk]                ; disk idx
	mov bx, copy_target           ; target pointer
	int 0x13

	cli                           ; disable interruptions before we define GTD
  lidt [idt_pointer]            ; load global descriptor table
	lgdt [gdt_pointer]            ; load global descriptor table
  call init_pic

	mov eax, cr0
	or eax, 0x01                  ; enable bit 0 of cr0 - protected mode
	mov cr0, eax

  mov ax, DATA_SEG              ; point to the new code selector
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	jmp CODE_SEG:boot2
gdt_start:
	dq 0x0                        ; first descriptor of GTD is null - 8 bytes
gdt_code:
	dw 0xffff                     ; segment length - 2 bytes
	dw 0x0                        ; segment base - 2 bytes
	db 0x0                        ; segment base - +1 byte
  ; flags - 1 byte
  ;  1  present
  ;  00 ring 0
  ;  1  code
  ;  1  executable
  ;  0  segment grows up - can only be executed from ring 0
  ;  1  read only bit
  ;  0  accessed. always zero here
	db 10011010b
  ; flags (4b) + segment length (4b) - 1 byte
  ;  1  granularity is 4KB (page granularity)
  ;  0  size bit. 0 for 16 bits, 1 for 32 bits protected mode
  ;  00 always zeros
  ;  segment length: 1111
	db 11001111b
	db 0x0                        ; segment base - 1 byte
gdt_data:
	dw 0xffff
	dw 0x0
	db 0x0
  ; flags - 1 byte
  ;  1  present
  ;  00 ring 0
  ;  1  code
  ;  0  data is non executable
  ;  0  segment grows up - can only be executed from ring 0
  ;  1  read only bit
  ;  0  accessed. always zero here
	db 10010010b
	db 11001111b
	db 0x0
gdt_end:
gdt_pointer:
	dw gdt_end - gdt_start
	dd gdt_start
disk:
	db 0x0

idt_pointer:
	dw 0
	dd 00

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

PIC1_COMM equ 0x20
PIC1_DATA equ PIC1_COMM + 1
PIC2_COMM equ 0xa0
PIC2_DATA equ PIC2_COMM + 1
ICW1 equ 0x11
ICW2_MASTER equ 0x20	; put IRQs 0-7 at 0x20 (above Intel reserved ints)
ICW2_SLAVE equ 0x28	; put IRQs 8-15 at 0x28
ICW3_MASTER equ 0x04	; IR2 connected to slave
ICW3_SLAVE equ 0x02	; slave has id 2
ICW4 equ 0x01		; 8086 mode, no auto-EOI, non-buffered mode,
			;   not special fully nested mode
io_wait:
	jmp	.done
  .done:
  ret

init_pic:
  ; Initialize master and slave PIC!
	mov	al, ICW1
	out	PIC1_COMM, al		; ICW1 to master
	call	io_wait
	out	PIC2_COMM, al		; ICW1 to slave
	call	io_wait
	mov	al, ICW2_MASTER
	out	PIC1_DATA, al		; ICW2 to master
	call	io_wait
	mov	al, ICW2_SLAVE
	out	PIC2_DATA, al		; ICW2 to slave
	call	io_wait
	mov	al, ICW3_MASTER
	out	PIC1_DATA, al		; ICW3 to master
	call	io_wait
	mov	al, ICW3_SLAVE
	out	PIC2_DATA, al		; ICW3 to slave
	call	io_wait
	mov	al, ICW4
	out	PIC1_DATA, al		; ICW4 to master
	call	io_wait
	out	PIC2_DATA, al		; ICW4 to slave
	call	io_wait
	mov	al, 0xff		; mask all ints in slave - change to 0
	out	PIC2_DATA, al		; OCW1 to slave
	call	io_wait
	mov	al, 0xff        ; mask all ints but 2 in master - change to 0
	out	PIC1_DATA, al		; OCW1 to master
	call	io_wait
  ret

times 400 - ($-$$) db 0        ;   ?b - pad binary with zeroes
db "MNOS"                      ;   4b - Unique Disk ID / Signature - MENI
dw 0x0                         ;   2b - Reserved 0x0000

; 16b - first partition - OS, user data, apps, etc
; 1MB reserved for boot, so let's start @ byte 1048576
; at 512b/sector we have sector 2048
; at 63sec/track we have track 32
db 0x80                        ;   1b - bootable or active
db 0x0                         ;   3b - starting head
db 0x00                        ;   6bits - starting sector
db 0x20                        ;  10bits - starting cylinder
db 0x06                        ;   1b - FAT16 (TODO: improve it later)
db 0xff                        ;   1b - ending head
dw 0xffff                      ;   2b - ending sector and cylinder
dd 0x00000800                  ;   4b - relative sector - 2048
dd 0xffffffff                  ;   4b - total sectors in partition

times 510 - ($-$$) db 0        ;  32b - pad binary with zeroes

dw 0xaa55                      ;   2b - valid bootsector

copy_target:

bits 32

extern idtp
extern irq_handler
extern isr_handler

idt_load:
  lidt [idtp] ; load interrupt descriptor table
  sti
  ret

ISR_NO_ERRNO 0
ISR_NO_ERRNO 1
ISR_NO_ERRNO 2
ISR_NO_ERRNO 3
ISR_NO_ERRNO 4
ISR_NO_ERRNO 5
ISR_NO_ERRNO 6
ISR_NO_ERRNO 7
ISR_ERRNO 8
ISR_NO_ERRNO 9
ISR_ERRNO 10
ISR_ERRNO 11
ISR_ERRNO 12
ISR_ERRNO 13
ISR_ERRNO 14
ISR_NO_ERRNO 15
ISR_NO_ERRNO 16
ISR_ERRNO 17
ISR_NO_ERRNO 18
ISR_NO_ERRNO 19
ISR_NO_ERRNO 20
ISR_NO_ERRNO 21
ISR_NO_ERRNO 22
ISR_NO_ERRNO 23
ISR_NO_ERRNO 24
ISR_NO_ERRNO 25
ISR_NO_ERRNO 26
ISR_NO_ERRNO 27
ISR_NO_ERRNO 28
ISR_NO_ERRNO 29
ISR_NO_ERRNO 30
ISR_NO_ERRNO 31
ISR_NO_ERRNO 128

IRQ 0, 32
IRQ 1, 33
; IRQ 2, 34
; IRQ 3, 35
; IRQ 4, 36
; IRQ 5, 37
; IRQ 6, 38
; IRQ 7, 39
; IRQ 8, 40
; IRQ 9, 41
; IRQ 10, 42
; IRQ 11, 43
; IRQ 12, 44
; IRQ 13, 45
; IRQ 14, 46
; IRQ 15, 47
; This is our common ISR stub. It saves the processor state, sets
; up for kernel mode segments, calls the C-level fault handler,
; and finally restores the stack frame.
isr_common_stub:
	pusha                    ; Pushes edi,esi,ebp,esp,ebx,edx,ecx,eax

	mov ax, ds               ; Lower 16-bits of eax = ds.
	push eax                 ; save the data segment descriptor

	mov ax, 0x10  ; load the kernel data segment descriptor
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	call isr_handler

	pop eax        ; reload the original data segment descriptor
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	popa                     ; Pops edi,esi,ebp...
	add esp, 8     ; Cleans up the pushed error code and pushed ISR number
	sti
	iret           ; pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP

; This is our common IRQ stub. It saves the processor state, sets
; up for kernel mode segments, calls the C-level fault handler,
; and finally restores the stack frame.
irq_common_stub:
	pusha                    ; Pushes edi,esi,ebp,esp,ebx,edx,ecx,eax

	mov ax, ds               ; Lower 16-bits of eax = ds.
	push eax                 ; save the data segment descriptor

	mov ax, 0x10  ; load the kernel data segment descriptor
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	call irq_handler

	pop ebx        ; reload the original data segment descriptor
	mov ds, bx
	mov es, bx
	mov fs, bx
	mov gs, bx

	popa           ; Pops edi,esi,ebp...
	add esp, 8     ; Cleans up the pushed error code and pushed ISR number
	sti
	iret           ; pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP

boot2:
  ; mov ax, 08h
  ; mov ds, ax
  ; mov ss, ax
	mov esp, kernel_stack_top
	extern kernel_start
	call kernel_start
	cli
	hlt

section .bss
align 4
kernel_stack_bottom: equ $
	resb 8096 ; 8 KB
kernel_stack_top:
