#include "heap.h"
#include "vmm.h"
#include "pmm.h"

typedef struct heap_block {
    size_t size;
    bool is_free;
    struct heap_block* next;
} heap_block_t;

#define ALIGN8(size) (((size) + 7) & ~7)

static heap_block_t* heap_start_block = NULL;
static size_t        heap_total_size  = 0;

void heap_init(uint32_t start_addr, size_t initial_size) {
    /* 1. Mapeia as paginas do heap via VMM e PMM */
    uint32_t pages = (initial_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t i = 0; i < pages; i++) {
        uint32_t virt = start_addr + (i * PAGE_SIZE);
        uint32_t phys = pmm_alloc_frame();
        vmm_map_page(phys, virt, PAGE_PRESENT | PAGE_WRITE);
    }

    /* 2. Inicializa o bloco mestre do Heap */
    heap_start_block = (heap_block_t*)start_addr;
    heap_start_block->size = initial_size - sizeof(heap_block_t);
    heap_start_block->is_free = true;
    heap_start_block->next = NULL;

    heap_total_size = initial_size;
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    size_t aligned_size = ALIGN8(size);
    heap_block_t* current = heap_start_block;

    /* First-Fit Search */
    while (current != NULL) {
        if (current->is_free && current->size >= aligned_size) {
            /* Verifica se vale a pena dividir o bloco (Split) */
            if (current->size >= aligned_size + sizeof(heap_block_t) + 16) {
                heap_block_t* next_block = (heap_block_t*)((uint32_t)current + sizeof(heap_block_t) + aligned_size);
                next_block->size = current->size - aligned_size - sizeof(heap_block_t);
                next_block->is_free = true;
                next_block->next = current->next;

                current->size = aligned_size;
                current->next = next_block;
            }

            current->is_free = false;
            return (void*)((uint32_t)current + sizeof(heap_block_t));
        }
        current = current->next;
    }

    return NULL; /* Sem memoria suficiente no heap */
}

void kfree(void* ptr) {
    if (ptr == NULL) return;

    heap_block_t* block = (heap_block_t*)((uint32_t)ptr - sizeof(heap_block_t));
    block->is_free = true;

    /* Fusao de blocos contiguos livres (Coalescing) */
    heap_block_t* current = heap_start_block;
    while (current != NULL && current->next != NULL) {
        if (current->is_free && current->next->is_free) {
            current->size += sizeof(heap_block_t) + current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}

void* kzalloc(size_t size) {
    void* ptr = kmalloc(size);
    if (ptr != NULL) {
        uint8_t* byte_ptr = (uint8_t*)ptr;
        for (size_t i = 0; i < size; i++) {
            byte_ptr[i] = 0;
        }
    }
    return ptr;
}

size_t heap_get_used_bytes(void) {
    size_t used = 0;
    heap_block_t* current = heap_start_block;
    while (current != NULL) {
        if (!current->is_free) {
            used += current->size + sizeof(heap_block_t);
        }
        current = current->next;
    }
    return used;
}

size_t heap_get_free_bytes(void) {
    size_t free_mem = 0;
    heap_block_t* current = heap_start_block;
    while (current != NULL) {
        if (current->is_free) {
            free_mem += current->size;
        }
        current = current->next;
    }
    return free_mem;
}
