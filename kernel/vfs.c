#include "vfs.h"
#include "heap.h"
#include "drivers.h"

static vfs_node_t* vfs_root = NULL;
static uint32_t    vfs_total_files = 0;

static bool str_equals(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] && s2[i]) {
        if (s1[i] != s2[i]) return false;
        i++;
    }
    return s1[i] == s2[i];
}

static size_t str_len(const char* s) {
    size_t len = 0;
    while (s && s[len]) len++;
    return len;
}

static void str_copy(char* dest, const char* src, size_t max_len) {
    size_t i = 0;
    while (src && src[i] && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static const char* get_filename_from_path(const char* path) {
    if (!path) return "unnamed";
    const char* last = path;
    int i = 0;
    while (path[i]) {
        if (path[i] == '/' || path[i] == '\\' || path[i] == ' ') {
            last = &path[i + 1];
        }
        i++;
    }
    return last;
}

vfs_node_t* vfs_get_root(void) {
    return vfs_root;
}

/* Cria um no basico */
static vfs_node_t* vfs_create_node(const char* name, vfs_node_type_t type, vfs_node_t* parent) {
    vfs_node_t* node = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    if (!node) return NULL;

    str_copy(node->name, name, sizeof(node->name));
    node->type     = type;
    node->size     = 0;
    node->data     = NULL;
    node->parent   = parent;
    node->children = NULL;
    node->next     = NULL;

    if (parent) {
        /* Insere no final da lista de filhos */
        if (!parent->children) {
            parent->children = node;
        } else {
            vfs_node_t* curr = parent->children;
            while (curr->next) {
                curr = curr->next;
            }
            curr->next = node;
        }
    }

    return node;
}

/* Busca um filho direto por nome dentro de um diretorio */
static vfs_node_t* vfs_find_child(vfs_node_t* dir, const char* name) {
    if (!dir || dir->type != VFS_TYPE_DIR) return NULL;

    if (str_equals(name, ".")) return dir;
    if (str_equals(name, "..")) return dir->parent ? dir->parent : dir;

    vfs_node_t* curr = dir->children;
    while (curr) {
        if (str_equals(curr->name, name)) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

/* Resolve qualquer caminho absoluto ou relativo a partir da raiz */
vfs_node_t* vfs_resolve_path(const char* path) {
    if (!path || !vfs_root) return NULL;

    while (*path == ' ') path++;
    if (*path == '\0' || str_equals(path, "/")) return vfs_root;

    vfs_node_t* current = vfs_root;

    char token[64];
    int p = 0;
    if (path[0] == '/') p++;

    while (path[p] != '\0') {
        int t = 0;
        while (path[p] != '/' && path[p] != '\0' && t < 63) {
            token[t++] = path[p++];
        }
        token[t] = '\0';

        if (t > 0) {
            vfs_node_t* next_node = vfs_find_child(current, token);
            if (!next_node) {
                return NULL; /* Caminho nao encontrado */
            }
            current = next_node;
        }

        if (path[p] == '/') p++;
    }

    return current;
}

/* Localiza o diretorio pai e extrai o nome do componente final */
static vfs_node_t* vfs_resolve_parent(const char* path, char* out_name, size_t max_name_len) {
    if (!path || !vfs_root) return NULL;

    /* Remove barras finais redundantes */
    char clean_path[256];
    str_copy(clean_path, path, sizeof(clean_path));
    size_t len = str_len(clean_path);
    while (len > 1 && clean_path[len - 1] == '/') {
        clean_path[--len] = '\0';
    }

    /* Encontra a ultima barra */
    int last_slash = -1;
    for (int i = 0; clean_path[i]; i++) {
        if (clean_path[i] == '/') last_slash = i;
    }

    if (last_slash == -1) {
        /* Nome no diretorio raiz */
        str_copy(out_name, clean_path, max_name_len);
        return vfs_root;
    } else if (last_slash == 0) {
        /* Ex: /arquivo ou /pasta */
        str_copy(out_name, &clean_path[1], max_name_len);
        return vfs_root;
    } else {
        clean_path[last_slash] = '\0';
        str_copy(out_name, &clean_path[last_slash + 1], max_name_len);
        return vfs_resolve_path(clean_path);
    }
}

vfs_node_t* vfs_mkdir(const char* path) {
    char dir_name[64];
    vfs_node_t* parent = vfs_resolve_parent(path, dir_name, sizeof(dir_name));
    if (!parent || parent->type != VFS_TYPE_DIR) return NULL;

    vfs_node_t* existing = vfs_find_child(parent, dir_name);
    if (existing) {
        if (existing->type == VFS_TYPE_DIR) return existing;
        return NULL; /* Ja existe arquivo com esse nome */
    }

    return vfs_create_node(dir_name, VFS_TYPE_DIR, parent);
}

vfs_node_t* vfs_create_file(const char* path, const uint8_t* data, uint32_t size) {
    char file_name[64];
    vfs_node_t* parent = vfs_resolve_parent(path, file_name, sizeof(file_name));
    if (!parent || parent->type != VFS_TYPE_DIR) return NULL;

    vfs_node_t* node = vfs_find_child(parent, file_name);
    if (node) {
        if (node->type != VFS_TYPE_FILE) return NULL;
        /* Se ja existe arquivo, substitui os dados */
        if (node->data) kfree(node->data);
    } else {
        node = vfs_create_node(file_name, VFS_TYPE_FILE, parent);
        if (!node) return NULL;
        vfs_total_files++;
    }

    node->size = size;
    if (size > 0 && data) {
        node->data = (uint8_t*)kmalloc(size + 1);
        if (node->data) {
            for (uint32_t i = 0; i < size; i++) {
                node->data[i] = data[i];
            }
            node->data[size] = '\0';
        }
    } else {
        node->data = NULL;
    }

    return node;
}

int32_t vfs_read_file(const char* path, void* buffer, uint32_t max_size) {
    vfs_node_t* node = vfs_resolve_path(path);
    if (!node || node->type != VFS_TYPE_FILE || !node->data || !buffer) return -1;

    uint32_t to_read = (node->size < max_size) ? node->size : max_size;
    uint8_t* dest = (uint8_t*)buffer;
    for (uint32_t i = 0; i < to_read; i++) {
        dest[i] = node->data[i];
    }
    return (int32_t)to_read;
}

int32_t vfs_write_file(const char* path, const void* data, uint32_t size) {
    vfs_node_t* node = vfs_create_file(path, (const uint8_t*)data, size);
    if (!node) return -1;
    return (int32_t)size;
}

int32_t vfs_stat(const char* path) {
    vfs_node_t* node = vfs_resolve_path(path);
    if (!node) return -1;
    return (node->type == VFS_TYPE_DIR) ? 1 : 0;
}

void vfs_list_dir(const char* path) {
    vfs_node_t* dir = vfs_resolve_path(path);
    if (!dir) {
        kprint("[VFS] Diretorio '");
        kprint(path ? path : "/");
        kprint("' nao encontrado.\n");
        return;
    }

    if (dir->type != VFS_TYPE_DIR) {
        kprint("[VFS] '");
        kprint(path);
        kprint("' nao e um diretorio.\n");
        return;
    }

    kprint("Conteudo de ");
    kprint(path && *path ? path : "/");
    kprint(":\n");

    vfs_node_t* curr = dir->children;
    if (!curr) {
        kprint("  (diretorio vazio)\n");
        return;
    }

    while (curr) {
        if (curr->type == VFS_TYPE_DIR) {
            kprint("  [DIR]  ");
            kprint(curr->name);
            kprint("/\n");
        } else {
            kprint("  [FILE] ");
            kprint(curr->name);
            kprint(" (");
            kprint_dec(curr->size);
            kprint(" bytes)\n");
        }
        curr = curr->next;
    }
}

/* Compatibilidade com o loader de binarios */
vfs_node_t* vfs_open(const char* name) {
    /* 1. Tenta resolver caminho direto */
    vfs_node_t* node = vfs_resolve_path(name);
    if (node && node->type == VFS_TYPE_FILE) return node;

    /* 2. Tenta dentro de /apps */
    char apps_path[128];
    apps_path[0] = '/';
    apps_path[1] = 'a';
    apps_path[2] = 'p';
    apps_path[3] = 'p';
    apps_path[4] = 's';
    apps_path[5] = '/';
    int i = 0;
    while (name[i] && i < 100) {
        apps_path[6 + i] = name[i];
        i++;
    }
    apps_path[6 + i] = '\0';

    node = vfs_resolve_path(apps_path);
    if (node && node->type == VFS_TYPE_FILE) return node;

    /* 3. Tenta adicionar extensao .bin */
    char with_bin[128];
    str_copy(with_bin, name, sizeof(with_bin));
    size_t len = str_len(with_bin);
    if (len < 120) {
        with_bin[len++] = '.';
        with_bin[len++] = 'b';
        with_bin[len++] = 'i';
        with_bin[len++] = 'n';
        with_bin[len] = '\0';
    }

    node = vfs_resolve_path(with_bin);
    if (node && node->type == VFS_TYPE_FILE) return node;

    /* Tenta /apps/nome.bin */
    str_copy(with_bin, apps_path, sizeof(with_bin));
    len = str_len(with_bin);
    if (len < 120) {
        with_bin[len++] = '.';
        with_bin[len++] = 'b';
        with_bin[len++] = 'i';
        with_bin[len++] = 'n';
        with_bin[len] = '\0';
    }
    return vfs_resolve_path(with_bin);
}

void vfs_list_files(void) {
    vfs_list_dir("/apps");
}

uint32_t vfs_get_count(void) {
    return vfs_total_files;
}

void vfs_init(multiboot_info_t* mbi) {
    /* 1. Cria o diretorio raiz '/' */
    vfs_root = vfs_create_node("/", VFS_TYPE_DIR, NULL);
    vfs_total_files = 0;

    /* 2. Cria estrutura padrao de diretorios */
    vfs_node_t* apps_dir = vfs_mkdir("/apps");
    vfs_mkdir("/docs");
    vfs_mkdir("/downloads");

    /* Cria um arquivo de boas-vindas inicial em /docs/readme.txt */
    const char* readme_text = 
        "Bem-vindo ao Vectra OS!\n"
        "Sistema com Microkernel x86 e VFS Hierarquico Real.\n"
        "Use 'mkdir', 'cd', 'ls' e 'cat' para explorar os arquivos!\n";
    vfs_create_file("/docs/readme.txt", (const uint8_t*)readme_text, str_len(readme_text));

    if (!(mbi->flags & MULTIBOOT_FLAG_MODS) || mbi->mods_count == 0 || mbi->mods_addr == 0) {
        kprint("[VFS] Nenhum modulo Multiboot detectado na imagem.\n");
        return;
    }

    multiboot_module_t* mods = (multiboot_module_t*)mbi->mods_addr;

    /* 3. Popula os modulos da ISO em /apps */
    for (uint32_t i = 0; i < mbi->mods_count; i++) {
        const char* clean_name = NULL;
        if (mods[i].cmdline != 0 && mods[i].cmdline != 0xFFFFFFFF && mods[i].cmdline < 0x08000000) {
            clean_name = get_filename_from_path((const char*)mods[i].cmdline);
        }

        if (!clean_name || *clean_name == '\0') {
            if (i == 0) clean_name = "shell.bin";
            else if (i == 1) clean_name = "hello.bin";
            else clean_name = "app.bin";
        }

        char file_name[64];
        int j = 0;
        while (clean_name[j] && clean_name[j] != ' ' && j < 63) {
            file_name[j] = clean_name[j];
            j++;
        }
        file_name[j] = '\0';

        uint32_t mod_size = mods[i].mod_end - mods[i].mod_start;
        uint8_t* mod_data = (uint8_t*)mods[i].mod_start;

        /* Cria o arquivo em /apps */
        char full_path[128];
        full_path[0] = '/';
        full_path[1] = 'a';
        full_path[2] = 'p';
        full_path[3] = 'p';
        full_path[4] = 's';
        full_path[5] = '/';
        int k = 0;
        while (file_name[k] && k < 60) {
            full_path[6 + k] = file_name[k];
            k++;
        }
        full_path[6 + k] = '\0';

        vfs_node_t* mod_node = vfs_create_node(file_name, VFS_TYPE_FILE, apps_dir);
        if (mod_node) {
            mod_node->size = mod_size;
            mod_node->data = mod_data; /* Aponta diretamente para o modulo em memoria */
            vfs_total_files++;
        }
    }
}
