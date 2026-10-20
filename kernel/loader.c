#include "loader.h"
#include "vfs.h"
#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "drivers.h"

static char last_cmdline[256] = "";

const char* loader_get_last_cmdline(void) {
    return last_cmdline;
}

int32_t bin_load_and_run(const char* app_name, bool blocking) {
    if (!app_name || !*app_name) return -1;

    /* Salva a linha de comando completa */
    int c = 0;
    while (app_name[c] && c < 255) {
        last_cmdline[c] = app_name[c];
        c++;
    }
    last_cmdline[c] = '\0';

    /* Isola o primeiro argumento (nome do binario) */
    char clean_name[64];
    int a = 0;
    while (app_name[a] && app_name[a] != ' ' && a < 63) {
        clean_name[a] = app_name[a];
        a++;
    }
    clean_name[a] = '\0';

    /* 1. Localiza o executavel no VFS */
    vfs_file_t* file = vfs_open(clean_name);
    if (!file) {
        kprint("[LOADER] Erro: Executavel '");
        kprint(clean_name);
        kprint("' nao encontrado no VFS.\n");
        return -1;
    }

    if (file->size == 0) {
        kprint("[LOADER] Erro: Executavel '");
        kprint(app_name);
        kprint("' possui tamanho 0.\n");
        return -1;
    }

    /* 2. Cria um espaco de enderecamento virtual (Page Directory) dedicado para o processo */
    uint32_t* task_pd = vmm_create_user_page_dir();
    if (!task_pd) {
        kprint("[LOADER] Falha ao alocar Page Directory para o app.\n");
        return -1;
    }

    /* 3. Aloca frames fisicos para codigo e dados/BSS (256 KiB) a partir de USER_CODE_VIRT_ADDR */
    uint32_t num_code_pages = (file->size + 4095) / 4096;
    uint32_t total_pages = num_code_pages + 64; /* Reserva 256 KiB para BSS e dados do app */
    for (uint32_t i = 0; i < total_pages; i++) {
        uint32_t code_frame = pmm_alloc_frame();
        if (!code_frame) {
            kprint("[LOADER] Falha de memoria fisica ao alocar paginas do processo.\n");
            return -1;
        }

        /* Copia codigo/dados do arquivo e zera BSS restante */
        uint8_t* frame_ptr = (uint8_t*)code_frame;
        size_t offset = i * 4096;
        for (size_t b = 0; b < 4096; b++) {
            if (offset + b < file->size) {
                frame_ptr[b] = file->data[offset + b];
            } else {
                frame_ptr[b] = 0;
            }
        }

        vmm_map_user_page(task_pd, code_frame, USER_CODE_VIRT_ADDR + (i * 4096), PAGE_USER | PAGE_WRITE | PAGE_PRESENT);
    }

    /* 4. Aloca 4 frames fisicos para a pilha de usuario (16 KiB) terminando em USER_STACK_VIRT_ADDR */
    for (uint32_t p = 1; p <= 4; p++) {
        uint32_t stack_frame = pmm_alloc_frame();
        if (!stack_frame) {
            kprint("[LOADER] Falha de memoria ao alocar pilha de usuario.\n");
            return -1;
        }

        uint8_t* frame_ptr = (uint8_t*)stack_frame;
        for (size_t b = 0; b < 4096; b++) {
            frame_ptr[b] = 0;
        }

        uint32_t stack_virt = USER_STACK_VIRT_ADDR - (p * 4096);
        vmm_map_user_page(task_pd, stack_frame, stack_virt, PAGE_USER | PAGE_WRITE | PAGE_PRESENT);
    }

    /* 5. Cria a tarefa de usuario em Ring 3 com seu proprio Page Directory */
    task_t* utask = task_create_user(file->name, USER_CODE_VIRT_ADDR, USER_STACK_VIRT_ADDR - 16, (uint32_t)task_pd);
    if (!utask) {
        kprint("[LOADER] Falha ao criar a tarefa de usuario para o app.\n");
        return -1;
    }

    kprint("[LOADER] App '");
    kprint(file->name);
    kprint("' carregado com sucesso (PID ");
    kprint_dec(utask->id);
    kprint(").\n");

    /* 6. Se blocking for verdadeiro, aguarda a finalizacao da tarefa filha */
    if (blocking) {
        while (utask->state != TASK_DEAD) {
            task_yield();
        }
        int32_t finished_id = (int32_t)utask->id;
        task_destroy(utask);
        return finished_id;
    }

    return (int32_t)utask->id;
}
