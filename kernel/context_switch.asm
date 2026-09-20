BITS 32

section .text
global context_switch

context_switch:
    mov eax, [esp+4]
    mov edx, [esp+8]

    pushfd
    push ebx
    push esi
    push edi
    push ebp

    mov [eax], esp

    mov esp, edx

    pop ebp
    pop edi
    pop esi
    pop ebx
    popfd

    ret
