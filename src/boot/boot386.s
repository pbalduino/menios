section .boot
bits 16

global boot
global idtp
global BOOTLOADER_SECTORS
global CODE_SEG
global DATA_SEG

boot:
  jmp 0:start

start:
  mov ax, 0x2401
  int 0x15

  mov [disk], dl                ; disk is given by BIOS
  mov ah, 0x02                  ; read sectors
  mov al, BOOTLOADER_SECTORS    ; sectors to read
  mov ch, 0x00                  ; cylinder idx
  mov dh, 0x00                  ; head idx
  mov cl, 0x02                  ; sector idx
  mov dl, [disk]                ; disk idx
  mov bx, copy_target           ; target pointer
  int 0x13

  cli                           ; disable interruptions before we define GTD
  lgdt [gdtp]                   ; load global descriptor table

  mov eax, cr0
	or eax, 0x01                  ; enable bit 0 of cr0 - protected mode
	mov cr0, eax

  lgdt [gdtp]                   ; load global descriptor table

  mov ax, DATA_SEG              ; point stack to the new data selector
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
gdtp:
	dw gdt_end - gdt_start
	dd gdt_start
disk:
	db 0x0

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start
BOOTLOADER_SECTORS equ 0x10

times 400 - ($-$$) db 0        ;   ?b - pad binary with zeroes
db "MNOS"                      ;   4b - Unique Disk ID / Signature - MENI
dw 0x0                         ;   2b - Reserved 0x0000

; 16b - first partition - kernel and descriptor
db 0x80                        ;   1b - bootable or active
db 0x0                         ;   3b - starting head
db 0x02                        ;   6bits - starting sector
db 0x01                        ;  10bits - starting cylinder
db 0x63                        ;   1b - BFS - Boot Filesystem
db 0xff                        ;   1b - ending head
dw 0xffff                      ;   2b - ending sector and cylinder
dd 0x0002                      ;   4b - relative sector - 2048
dd 0x4000                      ;   4b - total sectors in partition

; 16b - second partition - drivers, user data, apps, etc
; 8MB reserved for boot, so let's start @ byte 1048576
; at 512b/sector we have sector 2048
; at 63sec/track we have track 32
db 0x80                        ;   1b - bootable or active
db 0x0                         ;   3b - starting head
db 0x00                        ;   6bits - starting sector
db 0x20                        ;  10bits - starting cylinder
db 0x0c                        ;   1b - FAT32 LBA (TODO: improve it later)
db 0xff                        ;   1b - ending head
dw 0xffff                      ;   2b - ending sector and cylinder
dd 0x4002                      ;   4b - relative sector - 8193
dd 0xffffffff                  ;   4b - total sectors in partition

times 510 - ($-$$) db 0        ;  32b - pad binary with zeroes

dw 0xaa55                      ;   2b - valid bootsector

copy_target:

bits 32

boot2:
  extern init_kernel

  mov esp, kernel_stack_top
	call init_kernel
	cli
	hlt

section .bss
align 4
kernel_stack_bottom: equ $
	resb 0x2000  ; 8 KB
kernel_stack_top:
