BITS 32

global _start
extern kmain
extern __bss_start
extern __bss_end

section .text.entry
_start:
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    cld
    rep stosb

    call kmain

.hang:
    cli
    hlt
    jmp .hang
