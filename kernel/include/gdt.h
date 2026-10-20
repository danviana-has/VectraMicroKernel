#ifndef VECTRA_GDT_H
#define VECTRA_GDT_H

#include "types.h"

#define GDT_ENTRIES 6

/* Estrutura de cada entrada da GDT (8 bytes) */
typedef struct {
    uint16_t limit_low;     /* Bits 0..15 do limite */
    uint16_t base_low;      /* Bits 0..15 da base */
    uint8_t  base_middle;   /* Bits 16..23 da base */
    uint8_t  access;        /* Bytes de acesso e privilegios */
    uint8_t  granularity;   /* Granularidade e bits 16..19 do limite */
    uint8_t  base_high;     /* Bits 24..31 da base */
} __attribute__((packed)) gdt_entry_t;

/* Estrutura do ponteiro GDT para a instrucao lgdt */
typedef struct {
    uint16_t limit;         /* Tamanho da tabela GDT menos 1 */
    uint32_t base;          /* Endereco base linear da tabela */
} __attribute__((packed)) gdt_ptr_t;

/* Seletores de segmento do Vectra OS */
#define KERNEL_CS 0x08
#define KERNEL_DS 0x10
#define USER_CS   0x1B  /* 0x18 | 3 (RPL = 3) */
#define USER_DS   0x23  /* 0x20 | 3 (RPL = 3) */

void gdt_init(void);
void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);

#endif /* VECTRA_GDT_H */
