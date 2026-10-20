; ==============================================================================
; Vectra OS - Boot Entry Point (Multiboot 1 Compliant)
; Target Binary: kernel.akl (ELF32)
; ==============================================================================

MB_MAGIC    equ 0x1BADB002
MB_ALIGN    equ 1 << 0
MB_MEMINFO  equ 1 << 1
MB_FLAGS    equ MB_ALIGN | MB_MEMINFO
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384 ; 16 KiB de pilha de inicializacao para o Ring 0
stack_top:

section .text
global _start
extern kmain

_start:
    ; 1. Desativa interrupcoes mascaraveis
    cli

    ; 2. Configura a pilha inicial do kernel
    mov esp, stack_top

    ; 3. Reseta a flag de direcao (para operacoes com strings e ABI cdecl)
    cld

    ; 4. Passa os parametros fornecidos pelo bootloader Multiboot para kmain:
    ;    cdecl: argumentos sao empurrados da direita para a esquerda
    ;    kmain(uint32_t magic, uint32_t mbi_addr)
    push ebx ; Ponteiro para a estrutura de informacoes Multiboot
    push eax ; Magic number do Multiboot (0x2BADB002)

    ; 5. Transfere o controle para o Microkernel Core
    call kmain

    ; 6. Seguranca: loop infinito caso o kmain retorne
.halt:
    cli
    hlt
    jmp .halt
