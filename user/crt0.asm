[bits 32]

global _start
extern main

section .text
_start:
    xor ebp, ebp
    push ebp

    call main

    ; Executa SYS_EXIT diretamente (eax=0, ebx=codigo_de_retorno)
    mov ebx, eax
    mov eax, 0
    int 0x80

.hang:
    hlt
    jmp .hang
