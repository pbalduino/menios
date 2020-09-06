section .boot
bits 16
global boot
boot:
	mov ax, 0x2401
	int 0x15                      ; enable A20 bit

	mov [disk],dl                 ; disk is given by BIOS

	mov ah, 0x2                   ; read sectors
	mov al, 6                     ; sectors to read
	mov ch, 0                     ; cylinder idx
	mov dh, 0                     ; head idx
	mov cl, 2                     ; sector idx
	mov dl, [disk]                ; disk idx
	mov bx, copy_target           ; target pointer
	int 0x13
	cli                           ; disable interruptions before we define GTD
	lgdt [gdt_pointer]            ; load global descriptor table
	mov eax, cr0
	or eax,0x800000001            ; enable bit 1 of cr0 - protected mode
	mov cr0, eax                  ; enable bit 31 of cr0 - memory paging
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
	db 10011010b                  ; flags - 1 byte
	db 11001111b                  ; flags (4b) + segment length (4b) - 1 byte
	db 0x0                        ; segment base - 1 byte
gdt_data:
	dw 0xffff
	dw 0x0
	db 0x0
	db 10010010b
	db 11001111b
	db 0x0
gdt_end:
gdt_pointer:
	dw gdt_end - gdt_start
	dd gdt_start
disk:
	db 0x0
CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

times 510 - ($-$$) db 0        ; pad binary with zeroes
dw 0xaa55                      ; magic number says BIOS the sector is bootloader

copy_target:
bits 32

boot2:
	mov esp,kernel_stack_top
	extern kernel_start
	call kernel_start
	cli
	hlt

section .bss
align 4
kernel_stack_bottom: equ $
	resb 16384 ; 16 KB
kernel_stack_top:
