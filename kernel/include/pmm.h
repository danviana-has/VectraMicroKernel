#ifndef VECTRA_PMM_H
#define VECTRA_PMM_H

#include "types.h"
#include "multiboot.h"

#define PAGE_SIZE 4096

void pmm_init(multiboot_info_t* mbi);
uint32_t pmm_alloc_frame(void);
void pmm_free_frame(uint32_t phys_addr);

size_t pmm_get_total_memory(void);
size_t pmm_get_free_memory(void);
size_t pmm_get_used_memory(void);

#endif /* VECTRA_PMM_H */
