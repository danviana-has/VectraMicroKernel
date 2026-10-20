#ifndef VECTRA_TSS_H
#define VECTRA_TSS_H

#include "types.h"

/* Estrutura padrao do Task State Segment (TSS) da arquitetura x86 32-bit */
typedef struct {
    uint32_t prev_tss;   /* Link anterior se houver troca por hardware */
    uint32_t esp0;       /* Ponteiro de pilha Ring 0 (usado em interrupcoes e syscalls) */
    uint32_t ss0;        /* Seletor de segmento de pilha Ring 0 (0x10) */
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base; /* Offset do I/O Permission Bitmap */
} __attribute__((packed)) tss_entry_t;

void tss_init(uint32_t kernel_stack);
void tss_set_kernel_stack(uint32_t stack_addr);

#endif /* VECTRA_TSS_H */
