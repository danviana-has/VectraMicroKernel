#include "vmm.h"
#include "pmm.h"
#include "drivers.h"

#define INITIAL_TABLES 32 /* 32 tabelas x 4 MiB = 128 MiB mapeados no espaco virtual inicial */

/* Page Directory mestre do Kernel (1024 entradas = 4 KiB alinhado) */
static uint32_t kernel_page_directory[1024] __attribute__((aligned(4096)));

/* Tabelas de paginas iniciais para Identity Mapping dos primeiros 32 MiB */
static uint32_t initial_page_tables[INITIAL_TABLES][1024] __attribute__((aligned(4096)));

static inline void invlpg(uint32_t vaddr) {
    __asm__ volatile ("invlpg (%0)" : : "r"(vaddr) : "memory");
}

void vmm_map_page(uint32_t phys_addr, uint32_t virt_addr, uint32_t flags) {
    uint32_t pd_idx = PAGE_DIR_INDEX(virt_addr);
    uint32_t pt_idx = PAGE_TABLE_INDEX(virt_addr);

    uint32_t* page_table = NULL;

    if (kernel_page_directory[pd_idx] & PAGE_PRESENT) {
        page_table = (uint32_t*)(kernel_page_directory[pd_idx] & ~0xFFF);
        if (flags & PAGE_USER) {
            kernel_page_directory[pd_idx] |= PAGE_USER;
        }
    } else {
        uint32_t pt_phys = pmm_alloc_frame();
        if (!pt_phys) return;

        page_table = (uint32_t*)pt_phys;
        for (int i = 0; i < 1024; i++) {
            page_table[i] = 0;
        }

        kernel_page_directory[pd_idx] = pt_phys | PAGE_PRESENT | PAGE_WRITE | (flags & PAGE_USER);
    }

    page_table[pt_idx] = (phys_addr & ~0xFFF) | flags | PAGE_PRESENT;
    invlpg(virt_addr);
}

void vmm_unmap_page(uint32_t virt_addr) {
    uint32_t pd_idx = PAGE_DIR_INDEX(virt_addr);
    uint32_t pt_idx = PAGE_TABLE_INDEX(virt_addr);

    if (kernel_page_directory[pd_idx] & PAGE_PRESENT) {
        uint32_t* page_table = (uint32_t*)(kernel_page_directory[pd_idx] & ~0xFFF);
        page_table[pt_idx] = 0;
        invlpg(virt_addr);
    }
}

uint32_t vmm_get_phys_addr(uint32_t virt_addr) {
    uint32_t pd_idx = PAGE_DIR_INDEX(virt_addr);
    uint32_t pt_idx = PAGE_TABLE_INDEX(virt_addr);

    if (kernel_page_directory[pd_idx] & PAGE_PRESENT) {
        uint32_t* page_table = (uint32_t*)(kernel_page_directory[pd_idx] & ~0xFFF);
        if (page_table[pt_idx] & PAGE_PRESENT) {
            return (page_table[pt_idx] & ~0xFFF) | (virt_addr & 0xFFF);
        }
    }
    return 0;
}

void page_fault_handler(registers_t* regs) {
    uint32_t faulting_address;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(faulting_address));

    int present  = !(regs->err_code & 0x1);
    int rw       = regs->err_code & 0x2;
    int us       = regs->err_code & 0x4;
    int reserved = regs->err_code & 0x8;

    kprint("\n====================================================\n");
    kprint("[PANIC] EXCECAO 14: PAGE FAULT DETECTADO!\n");
    kprint("        Endereco virtual: 0x");
    char hex[9];
    for (int i = 7; i >= 0; i--) {
        uint8_t nibble = (faulting_address >> (i * 4)) & 0xF;
        hex[7 - i] = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
    }
    hex[8] = '\0';
    kprint(hex);
    kprint("\n        Causa: ");
    if (present) kprint("[Nao presente] ");
    if (rw) kprint("[Violacao escrita] ");
    if (us) kprint("[Ring 3] ");
    if (reserved) kprint("[Bits reservados] ");
    kprint("\n====================================================\n");

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void vmm_init(void) {
    /* 1. Inicializa o Page Directory */
    for (int i = 0; i < 1024; i++) {
        kernel_page_directory[i] = 0x00000002; /* RW, Nao presente */
    }

    /* 2. Identity Mapping dos primeiros 32 MiB (8 tabelas x 4 MiB) */
    for (int t = 0; t < INITIAL_TABLES; t++) {
        for (int p = 0; p < 1024; p++) {
            uint32_t phys_addr = (t * 1024 * 4096) + (p * 4096);
            initial_page_tables[t][p] = phys_addr | PAGE_PRESENT | PAGE_WRITE;
        }
        kernel_page_directory[t] = ((uint32_t)&initial_page_tables[t]) | PAGE_PRESENT | PAGE_WRITE;
    }

    /* 3. Registra o Handler de Page Fault (ISR 14) */
    register_interrupt_handler(14, page_fault_handler);

    /* 4. Carrega o Page Directory no CR3 */
    __asm__ volatile ("mov %0, %%cr3" : : "r"(&kernel_page_directory));

    /* 5. Ativa a Paginacao x86 setando o bit 31 (PG) do CR0 */
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));
}

uint32_t* vmm_create_user_page_dir(void) {
    uint32_t pd_phys = pmm_alloc_frame();
    if (!pd_phys) return NULL;

    uint32_t* pd = (uint32_t*)pd_phys;
    /* Copia os mapeamentos de identidade do kernel para que syscalls e IRQs funcionem */
    for (int i = 0; i < 1024; i++) {
        pd[i] = kernel_page_directory[i];
    }

    /* Limpa a regiao de espaco de usuario (PDE 256) */
    pd[PAGE_DIR_INDEX(0x40000000)] = 0x00000002;

    return pd;
}

void vmm_map_user_page(uint32_t* pd, uint32_t phys_addr, uint32_t virt_addr, uint32_t flags) {
    if (!pd) return;

    uint32_t pd_idx = PAGE_DIR_INDEX(virt_addr);
    uint32_t pt_idx = PAGE_TABLE_INDEX(virt_addr);
    uint32_t* page_table = NULL;

    if (pd[pd_idx] & PAGE_PRESENT) {
        page_table = (uint32_t*)(pd[pd_idx] & ~0xFFF);
        if (flags & PAGE_USER) {
            pd[pd_idx] |= PAGE_USER;
        }
    } else {
        uint32_t pt_phys = pmm_alloc_frame();
        if (!pt_phys) return;

        page_table = (uint32_t*)pt_phys;
        for (int i = 0; i < 1024; i++) {
            page_table[i] = 0;
        }

        pd[pd_idx] = pt_phys | PAGE_PRESENT | PAGE_WRITE | (flags & PAGE_USER);
    }

    page_table[pt_idx] = (phys_addr & ~0xFFF) | flags | PAGE_PRESENT;
}

