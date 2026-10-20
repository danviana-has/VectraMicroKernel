#ifndef VECTRA_VFS_H
#define VECTRA_VFS_H

#include "types.h"
#include "multiboot.h"

typedef enum {
    VFS_TYPE_FILE = 0,
    VFS_TYPE_DIR  = 1
} vfs_node_type_t;

typedef struct vfs_node {
    char                name[64];
    vfs_node_type_t     type;
    uint32_t            size;
    uint8_t*            data;       /* Dados do arquivo alocados dinamicamente */
    struct vfs_node*    parent;     /* Diretorio pai (NULL se for a raiz '/') */
    struct vfs_node*    children;   /* Primeiro filho (se DIR) */
    struct vfs_node*    next;       /* Proximo irmao no mesmo diretorio */
} vfs_node_t;

/* Alias para compatibilidade com o loader existente */
typedef vfs_node_t vfs_file_t;

void        vfs_init(multiboot_info_t* mbi);
vfs_node_t* vfs_get_root(void);
vfs_node_t* vfs_mkdir(const char* path);
vfs_node_t* vfs_create_file(const char* path, const uint8_t* data, uint32_t size);
vfs_node_t* vfs_resolve_path(const char* path);
vfs_node_t* vfs_open(const char* name);
int32_t     vfs_read_file(const char* path, void* buffer, uint32_t max_size);
int32_t     vfs_write_file(const char* path, const void* data, uint32_t size);
void        vfs_list_dir(const char* path);
int32_t     vfs_stat(const char* path);
void        vfs_list_files(void);
uint32_t    vfs_get_count(void);

#endif /* VECTRA_VFS_H */
