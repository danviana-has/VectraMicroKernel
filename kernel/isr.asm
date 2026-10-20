; ==============================================================================
; Vectra OS - Interrupt Service Routine (ISR) & IRQ Stubs (NASM ELF32)
; ==============================================================================

[bits 32]

extern isr_handler
extern irq_handler

; Macro para ISRs que NAO empurram codigo de erro automaticamente
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    cli
    push dword 0    ; Codigo de erro ficticio para manter a estrutura padronizada
    push dword %1   ; Numero da interrupcao
    jmp isr_common_stub
%endmacro

; Macro para ISRs que JA empurram codigo de erro pela CPU
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    cli
    push dword %1   ; Numero da interrupcao
    jmp isr_common_stub
%endmacro

; Macro para IRQs (Hardware Interrupts do PIC/APIC)
%macro IRQ 2
global irq%1
irq%1:
    cli
    push dword 0    ; Codigo de erro ficticio
    push dword %2   ; Numero remapeado (32 + %1)
    jmp irq_common_stub
%endmacro

; --- CPU Exceptions (ISRs 0..31) ---
ISR_NOERRCODE 0   ; Divide by zero
ISR_NOERRCODE 1   ; Debug
ISR_NOERRCODE 2   ; Non-maskable Interrupt
ISR_NOERRCODE 3   ; Breakpoint
ISR_NOERRCODE 4   ; Overflow
ISR_NOERRCODE 5   ; Bound Range Exceeded
ISR_NOERRCODE 6   ; Invalid Opcode
ISR_NOERRCODE 7   ; Device Not Available
ISR_ERRCODE   8   ; Double Fault (empurra erro)
ISR_NOERRCODE 9   ; Coprocessor Segment Overrun
ISR_ERRCODE   10  ; Invalid TSS (empurra erro)
ISR_ERRCODE   11  ; Segment Not Present (empurra erro)
ISR_ERRCODE   12  ; Stack-Segment Fault (empurra erro)
ISR_ERRCODE   13  ; General Protection Fault (empurra erro)
ISR_ERRCODE   14  ; Page Fault (empurra erro)
ISR_NOERRCODE 15  ; Reserved
ISR_NOERRCODE 16  ; x87 Floating-Point Exception
ISR_ERRCODE   17  ; Alignment Check (empurra erro)
ISR_NOERRCODE 18  ; Machine Check
ISR_NOERRCODE 19  ; SIMD Floating-Point Exception
ISR_NOERRCODE 20  ; Virtualization Exception
ISR_ERRCODE   21  ; Control Protection Exception (empurra erro)
ISR_NOERRCODE 22  ; Reserved
ISR_NOERRCODE 23  ; Reserved
ISR_NOERRCODE 24  ; Reserved
ISR_NOERRCODE 25  ; Reserved
ISR_NOERRCODE 26  ; Reserved
ISR_NOERRCODE 27  ; Reserved
ISR_NOERRCODE 28  ; Hypervisor Injection Exception
ISR_ERRCODE   29  ; VMM Communication Exception (empurra erro)
ISR_ERRCODE   30  ; Security Exception (empurra erro)
ISR_NOERRCODE 31  ; Reserved

; --- Hardware IRQs (32..47) ---
IRQ 0,  32  ; PIT Timer
IRQ 1,  33  ; Teclado PS/2
IRQ 2,  34  ; Cascade PIC2
IRQ 3,  35  ; COM2
IRQ 4,  36  ; COM1
IRQ 5,  37  ; LPT2
IRQ 6,  38  ; Floppy
IRQ 7,  39  ; LPT1 / Spurious
IRQ 8,  40  ; CMOS RTC
IRQ 9,  41  ; Free / NIC
IRQ 10, 42  ; Free
IRQ 11, 43  ; Free
IRQ 12, 44  ; Mouse PS/2
IRQ 13, 45  ; FPU Coprocessor
IRQ 14, 46  ; Primary ATA
IRQ 15, 47  ; Secondary ATA

; Syscall Gate (int 0x80 = 128)
ISR_NOERRCODE 128

; --- Stub Comum para Excecoes (ISRs) ---
isr_common_stub:
    pusha           ; Empurra edi, esi, ebp, esp, ebx, edx, ecx, eax
    mov ax, ds      ; Salva o descritor de dados
    push eax

    mov ax, 0x10    ; Carrega o segmento de dados do kernel
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp        ; Passa o ponteiro da estrutura registers_t como argumento
    call isr_handler
    add esp, 4      ; Limpa o argumento da chamada C

    pop eax         ; Restaura os registradores de segmento originais
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa            ; Restaura edi, esi, ebp, etc.
    add esp, 8      ; Limpa o int_no e o codigo de erro empurrados
    iret

; --- Stub Comum para IRQs ---
irq_common_stub:
    pusha
    mov ax, ds
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call irq_handler
    add esp, 4

    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8
    iret

; --- Carregador da IDT ---
global idt_flush
idt_flush:
    mov eax, [esp + 4]
    lidt [eax]
    ret
