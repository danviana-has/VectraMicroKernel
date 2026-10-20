#include "gdt.h"

static gdt_entry_t gdt_entries[GDT_ENTRIES];
static gdt_ptr_t   gdt_ptr;

/* Recarrega a GDT e os registradores de segmento */
static void gdt_flush(uint32_t ptr_addr) {
    __asm__ volatile (
        "lgdt (%0)\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        "ljmp $0x08, $.reload_cs\n\t"
        ".reload_cs:\n\t"
        :
        : "r" (ptr_addr)
        : "eax", "memory"
    );
}

void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;

    gdt_entries[num].granularity |= (gran & 0xF0);
    gdt_entries[num].access      = access;
}

void gdt_init(void) {
    gdt_ptr.limit = (sizeof(gdt_entry_t) * GDT_ENTRIES) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;

    /* 0x00: Null Descriptor */
    gdt_set_gate(0, 0, 0, 0, 0);

    /* 0x08: Kernel Code Segment (Base=0, Limit=4GB, Ring 0, RX, 32-bit, Gran 4K) */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    /* 0x10: Kernel Data Segment (Base=0, Limit=4GB, Ring 0, RW, 32-bit, Gran 4K) */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    /* 0x18: User Code Segment (Base=0, Limit=4GB, Ring 3, RX, 32-bit, Gran 4K) */
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    /* 0x20: User Data Segment (Base=0, Limit=4GB, Ring 3, RW, 32-bit, Gran 4K) */
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    /* 0x28: Reservado para TSS (Modulo 1.3) */
    gdt_set_gate(5, 0, 0, 0, 0);

    /* Instala a GDT e faz o far jump para carregar o novo CS */
    gdt_flush((uint32_t)&gdt_ptr);
}
