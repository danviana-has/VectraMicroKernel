#include "tss.h"
#include "gdt.h"

static tss_entry_t kernel_tss;

/* Carrega o registrador de tarefa (TR) da CPU com o seletor da GDT */
static inline void ltr(uint16_t sel) {
    __asm__ volatile ("ltr %0" : : "r"(sel));
}

void tss_init(uint32_t kernel_stack) {
    uint8_t* ptr = (uint8_t*)&kernel_tss;
    for (size_t i = 0; i < sizeof(tss_entry_t); i++) {
        ptr[i] = 0;
    }

    /* O SS0 deve ser o segmento de dados do kernel (0x10) */
    kernel_tss.ss0        = KERNEL_DS;
    kernel_tss.esp0       = kernel_stack;
    kernel_tss.cs         = KERNEL_CS | 3;
    kernel_tss.ss         = KERNEL_DS | 3;
    kernel_tss.ds         = KERNEL_DS | 3;
    kernel_tss.es         = KERNEL_DS | 3;
    kernel_tss.fs         = KERNEL_DS | 3;
    kernel_tss.gs         = KERNEL_DS | 3;
    kernel_tss.iomap_base = sizeof(tss_entry_t);

    /* Instala o descritor TSS na entrada 5 da GDT (Seletor 0x28) */
    /* 0x89 = 10001001b (Present, Ring 0, 32-bit Available TSS) */
    gdt_set_gate(5, (uint32_t)&kernel_tss, sizeof(tss_entry_t) - 1, 0x89, 0x00);

    /* Carrega o seletor no TR */
    ltr(0x28);
}

void tss_set_kernel_stack(uint32_t stack_addr) {
    kernel_tss.esp0 = stack_addr;
}
