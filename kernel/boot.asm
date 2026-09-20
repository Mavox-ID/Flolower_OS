BITS 16
ORG 0x7C00

KERNEL_LOAD_SEGMENT equ 0x1000
KERNEL_SECTORS      equ 400

VBE_INFO_BUF    equ 0x0500
VBE_PARAMS_ADDR equ 0x0600

VESA_MODE equ 0x4115

DAP_ADDR equ 0x0610

CHUNK_SECTORS equ 64

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]
    int 0x13
    jc no_extensions
    cmp bx, 0xAA55
    jne no_extensions

    mov word [sectors_left], KERNEL_SECTORS
    mov dword [cur_lba], 1
    mov word [cur_segment], KERNEL_LOAD_SEGMENT

read_loop:
    cmp word [sectors_left], 0
    je read_done

    mov ax, [sectors_left]
    cmp ax, CHUNK_SECTORS
    jbe .chunk_size_ok
    mov ax, CHUNK_SECTORS
.chunk_size_ok:
    mov [this_chunk], ax

    mov di, DAP_ADDR
    mov byte [di + 0], 0x10
    mov byte [di + 1], 0
    mov ax, [this_chunk]
    mov [di + 2], ax
    mov word [di + 4], 0
    mov ax, [cur_segment]
    mov [di + 6], ax
    mov eax, [cur_lba]
    mov [di + 8], eax
    mov dword [di + 12], 0

    mov si, DAP_ADDR
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    movzx eax, word [this_chunk]
    add [cur_lba], eax
    sub [sectors_left], ax

    mov ax, [this_chunk]
    shl ax, 5
    add [cur_segment], ax

    jmp read_loop

read_done:

    xor ax, ax
    mov es, ax
    mov di, VBE_INFO_BUF

    mov ax, 0x4F01
    mov cx, VESA_MODE
    int 0x10
    cmp ax, 0x004F
    jne vesa_failed

    mov ax, 0x4F02
    mov bx, VESA_MODE
    int 0x10
    cmp ax, 0x004F
    jne vesa_failed

    mov si, VBE_INFO_BUF
    mov eax, [si + 0x28]
    mov [VBE_PARAMS_ADDR], eax

    mov ax, [si + 0x10]
    mov [VBE_PARAMS_ADDR + 4], ax

    mov al, [si + 0x20]
    mov [VBE_PARAMS_ADDR + 8], al
    mov al, [si + 0x22]
    mov [VBE_PARAMS_ADDR + 9], al
    mov al, [si + 0x24]
    mov [VBE_PARAMS_ADDR + 10], al

    mov word [VBE_PARAMS_ADDR + 6], 1
    jmp vesa_done

vesa_failed:
    mov word [VBE_PARAMS_ADDR + 6], 0
vesa_done:

    in al, 0x92
    or al, 2
    out 0x92, al

    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp CODE_SEG:protected_mode_entry

no_extensions:
    mov si, no_ext_msg
    jmp print_and_hang
disk_error:
    mov si, disk_err_msg
print_and_hang:
.print:
    lodsb
    or al, al
    jz .hang
    mov ah, 0x0E
    int 0x10
    jmp .print
.hang:
    hlt
    jmp .hang

disk_err_msg db "Flolower boot: disk read error", 0
no_ext_msg   db "Flolower boot: BIOS lacks INT13h extensions (LBA)", 0
boot_drive db 0
sectors_left dw 0
cur_lba     dd 0
cur_segment dw 0
this_chunk  dw 0

gdt_start:
gdt_null:
    dq 0
gdt_code:
    dw 0xFFFF, 0
    db 0, 10011010b, 11001111b, 0
gdt_data:
    dw 0xFFFF, 0
    db 0, 10010010b, 11001111b, 0
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

BITS 32
protected_mode_entry:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x700000

    jmp 0x10000

times 510-($-$$) db 0
dw 0xAA55
