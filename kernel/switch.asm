; ==============================================================================
; Vectra OS - Low-Level Context Switch Routine (NASM ELF32)
; ==============================================================================

[bits 32]

global task_switch_asm
extern tss_set_kernel_stack

; void task_switch_asm(uint32_t* old_esp, uint32_t new_esp, uint32_t new_cr3, uint32_t kernel_stack);
task_switch_asm:
    ; 1. Salva os registradores do contexto da tarefa que esta saindo
    pushfd
    push ebp
    push edi
    push esi
    push ebx

    ; 2. Localiza os argumentos passados na pilha:
    ;    [esp + 24] = old_esp
    ;    [esp + 28] = new_esp
    ;    [esp + 32] = new_cr3
    ;    [esp + 36] = kernel_stack
    mov eax, [esp + 24]
    mov [eax], esp          ; Salva o esp atual na estrutura da tarefa anterior

    mov ebx, [esp + 28]     ; new_esp lido com seguranca ANTES de trocar CR3
    mov ecx, [esp + 32]     ; new_cr3
    mov edx, [esp + 36]     ; kernel_stack

    push edx
    call tss_set_kernel_stack
    add esp, 4

    mov edx, cr3
    cmp edx, ecx
    je .same_cr3
    mov cr3, ecx            ; Troca o espaco de enderecamento virtual se diferente
.same_cr3:

    mov esp, ebx            ; Carrega o esp da nova tarefa sem acessar stack antiga apos mudar CR3

    ; 3. Restaura o contexto da nova tarefa
    pop ebx
    pop esi
    pop edi
    pop ebp
    popfd

    ret                     ; Retorna para o ponto onde a nova tarefa parou

; ==============================================================================
; user_jump_trampoline: Executado na primeira ativacao de uma tarefa Ring 3
; Espera que a pilha contenha: [EIP, CS, EFLAGS, ESP, SS]
; ==============================================================================
global user_jump_trampoline
user_jump_trampoline:
    mov ax, 0x23            ; User Data Segment (RPL 3)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    iret                    ; Transicao por hardware para Ring 3

