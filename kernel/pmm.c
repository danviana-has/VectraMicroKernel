#include "pmm.h"

extern uint32_t _kernel_start;
extern uint32_t _kernel_end;

static uint32_t* pmm_bitmap;
static uint32_t  pmm_max_frames;
static uint32_t  pmm_used_frames;
static uint32_t  pmm_total_memory_bytes;

#define BITMAP_INDEX(frame) ((frame) / 32)
#define BITMAP_OFFSET(frame) ((frame) % 32)

static inline void bitmap_set(uint32_t frame) {
    if (frame < pmm_max_frames) {
        if (!(pmm_bitmap[BITMAP_INDEX(frame)] & (1 << BITMAP_OFFSET(frame)))) {
            pmm_bitmap[BITMAP_INDEX(frame)] |= (1 << BITMAP_OFFSET(frame));
            pmm_used_frames++;
        }
    }
}

static inline void bitmap_unset(uint32_t frame) {
    if (frame < pmm_max_frames) {
        if (pmm_bitmap[BITMAP_INDEX(frame)] & (1 << BITMAP_OFFSET(frame))) {
            pmm_bitmap[BITMAP_INDEX(frame)] &= ~(1 << BITMAP_OFFSET(frame));
            pmm_used_frames--;
        }
    }
}

static inline bool bitmap_test(uint32_t frame) {
    if (frame >= pmm_max_frames) return true;
    return (pmm_bitmap[BITMAP_INDEX(frame)] & (1 << BITMAP_OFFSET(frame))) != 0;
}

static int32_t bitmap_find_first_free(void) {
    for (uint32_t i = 0; i < pmm_max_frames / 32; i++) {
        if (pmm_bitmap[i] != 0xFFFFFFFF) {
            for (uint8_t j = 0; j < 32; j++) {
                if (!(pmm_bitmap[i] & (1 << j))) {
                    return (i * 32) + j;
                }
            }
        }
    }
    return -1;
}

void pmm_init(multiboot_info_t* mbi) {
    uint32_t highest_address = 0;

    /* 1. Inspeciona o mapa de memoria Multiboot para calcular a memoria maxima */
    if (mbi->flags & MULTIBOOT_FLAG_MMAP) {
        multiboot_memory_map_t* mmap = (multiboot_memory_map_t*)mbi->mmap_addr;
        uint32_t mmap_end = mbi->mmap_addr + mbi->mmap_length;

        while ((uint32_t)mmap < mmap_end) {
            if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
                uint32_t end_addr = mmap->addr_low + mmap->len_low;
                if (end_addr > highest_address) {
                    highest_address = end_addr;
                }
            }
            mmap = (multiboot_memory_map_t*)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
        }
    } else {
        /* Fallback: usa mem_upper (em KiB) */
        highest_address = 0x100000 + (mbi->mem_upper * 1024);
    }

    pmm_total_memory_bytes = highest_address;
    pmm_max_frames = highest_address / PAGE_SIZE;

    /* 2. O bitmap e colocado apos o kernel e quaisquer modulos Multiboot carregados */
    uint32_t kernel_end_addr = (uint32_t)&_kernel_end;

    if ((mbi->flags & MULTIBOOT_FLAG_MODS) && mbi->mods_count > 0 && mbi->mods_addr != 0) {
        multiboot_module_t* mods = (multiboot_module_t*)mbi->mods_addr;
        uint32_t mods_struct_end = mbi->mods_addr + (mbi->mods_count * sizeof(multiboot_module_t));
        if (mods_struct_end > kernel_end_addr) {
            kernel_end_addr = mods_struct_end;
        }
        for (uint32_t m = 0; m < mbi->mods_count; m++) {
            if (mods[m].mod_end > kernel_end_addr) {
                kernel_end_addr = mods[m].mod_end;
            }
        }
    }

    pmm_bitmap = (uint32_t*)((kernel_end_addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
    uint32_t bitmap_size_bytes = (pmm_max_frames / 8) + 1;
    uint32_t bitmap_end_addr = (uint32_t)pmm_bitmap + bitmap_size_bytes;

    /* 3. Marca inicialmente toda a memoria como ocupada */
    for (uint32_t i = 0; i < (pmm_max_frames / 32) + 1; i++) {
        pmm_bitmap[i] = 0xFFFFFFFF;
    }
    pmm_used_frames = pmm_max_frames;

    /* 4. Libera apenas as regioes marcadas como usaveis pela BIOS */
    if (mbi->flags & MULTIBOOT_FLAG_MMAP) {
        multiboot_memory_map_t* mmap = (multiboot_memory_map_t*)mbi->mmap_addr;
        uint32_t mmap_end = mbi->mmap_addr + mbi->mmap_length;

        while ((uint32_t)mmap < mmap_end) {
            if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
                uint32_t start_frame = mmap->addr_low / PAGE_SIZE;
                uint32_t frame_count = mmap->len_low / PAGE_SIZE;

                for (uint32_t f = 0; f < frame_count; f++) {
                    bitmap_unset(start_frame + f);
                }
            }
            mmap = (multiboot_memory_map_t*)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
        }
    }

    /* 5. Reserva o primeiro 1 MiB (IVT, BIOS, VGA text buffer) */
    for (uint32_t f = 0; f < (0x100000 / PAGE_SIZE); f++) {
        bitmap_set(f);
    }

    /* 6. Reserva o espaco ocupado pelo proprio Kernel e pelo Bitmap */
    uint32_t reserved_start = 0x100000 / PAGE_SIZE;
    uint32_t reserved_end   = ((bitmap_end_addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)) / PAGE_SIZE;

    for (uint32_t f = reserved_start; f < reserved_end; f++) {
        bitmap_set(f);
    }
}

uint32_t pmm_alloc_frame(void) {
    int32_t free_frame = bitmap_find_first_free();
    if (free_frame == -1) {
        return 0; /* Falha de alocacao: sem memoria fisica */
    }

    bitmap_set(free_frame);
    return (uint32_t)(free_frame * PAGE_SIZE);
}

void pmm_free_frame(uint32_t phys_addr) {
    uint32_t frame = phys_addr / PAGE_SIZE;
    bitmap_unset(frame);
}

size_t pmm_get_total_memory(void) {
    return pmm_total_memory_bytes;
}

size_t pmm_get_free_memory(void) {
    return (pmm_max_frames - pmm_used_frames) * PAGE_SIZE;
}

size_t pmm_get_used_memory(void) {
    return pmm_used_frames * PAGE_SIZE;
}
