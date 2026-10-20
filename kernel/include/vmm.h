#ifndef VECTRA_VMM_H
#define VECTRA_VMM_H

#include "types.h"
#include "idt.h"

#define PAGE_PRESENT  0x01
#define PAGE_WRITE    0x02
#define PAGE_USER     0x04

#define PAGE_DIR_INDEX(vaddr)   (((vaddr) >> 22) & 0x3FF)
#define PAGE_TABLE_INDEX(vaddr) (((vaddr) >> 12) & 0x3FF)

void vmm_init(void);
void vmm_map_page(uint32_t phys_addr, uint32_t virt_addr, uint32_t flags);
void vmm_unmap_page(uint32_t virt_addr);
uint32_t vmm_get_phys_addr(uint32_t virt_addr);
void page_fault_handler(registers_t* regs);

uint32_t* vmm_create_user_page_dir(void);
void vmm_map_user_page(uint32_t* pd, uint32_t phys_addr, uint32_t virt_addr, uint32_t flags);

#endif /* VECTRA_VMM_H */

