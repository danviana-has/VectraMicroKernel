#ifndef VECTRA_HEAP_H
#define VECTRA_HEAP_H

#include "types.h"

void  heap_init(uint32_t start_addr, size_t initial_size);
void* kmalloc(size_t size);
void* kzalloc(size_t size);
void  kfree(void* ptr);

size_t heap_get_used_bytes(void);
size_t heap_get_free_bytes(void);

#endif /* VECTRA_HEAP_H */
