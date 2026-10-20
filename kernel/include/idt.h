#ifndef VECTRA_IDT_H
#define VECTRA_IDT_H

#include "types.h"

#define IDT_ENTRIES 256

/* Estrutura de cada entrada da IDT (Gate Descriptor) */
typedef struct {
    uint16_t base_low;   /* Bits 0..15 do offset do handler */
    uint16_t sel;        /* Seletor de segmento de codigo (0x08) */
    uint8_t  always0;    /* Sempre 0 */
    uint8_t  flags;      /* Flags de presenca, DPL e tipo de gate */
    uint16_t base_high;  /* Bits 16..31 do offset do handler */
} __attribute__((packed)) idt_entry_t;

/* Ponteiro da IDT para a instrucao lidt */
typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

/* Estrutura com os registradores salvos na pilha durante a interrupcao */
typedef struct {
    uint32_t ds;                                      /* Data segment selector */
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;  /* Salvos por pusha */
    uint32_t int_no, err_code;                        /* Numero da interrupcao e codigo de erro */
    uint32_t eip, cs, eflags, useresp, ss;            /* Salvos automaticamente pelo hardware */
} registers_t;

typedef void (*isr_handler_t)(registers_t* regs);

void idt_init(void);
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
void register_interrupt_handler(uint8_t n, isr_handler_t handler);

#endif /* VECTRA_IDT_H */
