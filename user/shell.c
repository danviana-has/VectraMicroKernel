#include "include/syscall.h"

#define MAX_CMD_LEN  128
#define MAX_PATH_LEN 128
#define FILE_BUF_LEN 16384

static char cwd[MAX_PATH_LEN] = "/";
static char file_buf[FILE_BUF_LEN];

static int str_cmp(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] && s2[i]) {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        i++;
    }
    return s1[i] - s2[i];
}

static int str_len(const char* s) {
    int len = 0;
    while (s && s[len]) len++;
    return len;
}

static void str_copy(char* dest, const char* src, int max_len) {
    int i = 0;
    while (src && src[i] && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static int str_starts_with(const char* str, const char* prefix) {
    int i = 0;
    while (prefix[i]) {
        if (str[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}

static void print_char(char c) {
    char buf[2];
    buf[0] = c;
    buf[1] = '\0';
    sys_print(buf);
}

static void print_num(int num) {
    if (num == 0) {
        print_char('0');
        return;
    }
    char buf[16];
    int i = 14;
    buf[15] = '\0';
    int n = num;
    if (n < 0) n = -n;
    while (n > 0) {
        buf[i--] = '0' + (n % 10);
        n /= 10;
    }
    if (num < 0) buf[i--] = '-';
    sys_print(&buf[i + 1]);
}

static void read_line(char* buffer, int max_len) {
    int idx = 0;
    while (idx < max_len - 1) {
        int ch = sys_getchar(1); /* Bloqueante */
        if (ch == '\r' || ch == '\n') {
            sys_print("\n");
            break;
        } else if (ch == '\b' || ch == 127) {
            if (idx > 0) {
                idx--;
                sys_print("\b \b");
            }
        } else if (ch >= 32 && ch <= 126) {
            buffer[idx++] = (char)ch;
            print_char((char)ch);
        }
    }
    buffer[idx] = '\0';
}

/* Constroi um caminho absoluto canonico a partir de cwd e caminho relativo/absoluto */
static void build_full_path(const char* rel_path, char* out_path, int max_len) {
    while (*rel_path == ' ') rel_path++;

    if (*rel_path == '/') {
        str_copy(out_path, rel_path, max_len);
    } else {
        /* Concatena cwd + rel_path */
        int cur_len = str_len(cwd);
        str_copy(out_path, cwd, max_len);
        if (cur_len > 1 && out_path[cur_len - 1] != '/') {
            if (cur_len < max_len - 2) {
                out_path[cur_len++] = '/';
                out_path[cur_len] = '\0';
            }
        }
        int i = 0;
        while (rel_path[i] && cur_len < max_len - 1) {
            out_path[cur_len++] = rel_path[i++];
        }
        out_path[cur_len] = '\0';
    }

    /* Remove barras finais se nao for apenas "/" */
    int len = str_len(out_path);
    while (len > 1 && out_path[len - 1] == '/') {
        out_path[--len] = '\0';
    }
}

int main(void) {
    sys_clear();
    sys_print("=======================================================\n");
    sys_print("  VECTRA OS - INTERACTIVE SHELL (RING 3 / USER-SPACE)  \n");
    sys_print("  Executando em modo de usuario com privilegios DPL=3  \n");
    sys_print("  VFS Hierarquico ativado: explore com cd, ls, cat     \n");
    sys_print("  Digite 'help' para exibir os comandos disponiveis.   \n");
    sys_print("=======================================================\n\n");

    char cmd[MAX_CMD_LEN];

    for (;;) {
        /* Prompt exibindo usuario, nivel de privilegio e diretorio atual */
        sys_print("vectra@ring3:");
        sys_print(cwd);
        sys_print("$ ");

        read_line(cmd, MAX_CMD_LEN);

        /* Remove espacos iniciais */
        char* p = cmd;
        while (*p == ' ') p++;
        if (*p == '\0') continue;

        if (str_cmp(p, "help") == 0) {
            sys_print("Comandos disponiveis no Shell:\n");
            sys_print("  help                 - Exibe esta mensagem de ajuda\n");
            sys_print("  pwd                  - Exibe o caminho do diretorio de trabalho atual\n");
            sys_print("  ls [dir]             - Lista arquivos e pastas no diretorio atual ou indicado\n");
            sys_print("  cd <dir>             - Muda o diretorio de trabalho atual (ex: cd docs, cd ..)\n");
            sys_print("  mkdir <dir>          - Cria um novo diretorio real no VFS\n");
            sys_print("  cat <arquivo>        - Exibe o conteudo de um arquivo de texto no VFS\n");
            sys_print("  write <arq> <txt>    - Grava texto em um arquivo no VFS\n");
            sys_print("  notepad -newfile <A> - Cria e edita novo arquivo no editor estilo MS-DOS\n");
            sys_print("  notepad -editfile <A>- Abre e edita arquivo existente no editor estilo MS-DOS\n");
            sys_print("  downloadfile -web <U>- Baixa arquivo da Web real e salva no VFS (/downloads/)\n");
            sys_print("  netinfo              - Exibe status e IP da placa de rede\n");
            sys_print("  apps                 - Atalho para listar executaveis .bin no disco\n");
            sys_print("  exec -app <NOME>     - Carrega e executa o binario externo em Ring 3\n");
            sys_print("  beep                 - Testa o driver PC Speaker emitindo som\n");
            sys_print("  nvram                - Testa o driver de leitura e escrita NVRAM/CMOS\n");
            sys_print("  echo <texto>         - Imprime o texto fornecido\n");
            sys_print("  clear                - Limpa a tela do console\n");
            sys_print("  exit                 - Finaliza a sessao do shell\n");
        }
        else if (str_cmp(p, "pwd") == 0) {
            sys_print(cwd);
            sys_print("\n");
        }
        else if (str_cmp(p, "ls") == 0 || str_starts_with(p, "ls ")) {
            char target[MAX_PATH_LEN];
            char* arg = p + 2;
            while (*arg == ' ') arg++;
            if (*arg == '\0') {
                str_copy(target, cwd, sizeof(target));
            } else {
                build_full_path(arg, target, sizeof(target));
            }
            sys_list_dir(target);
        }
        else if (str_cmp(p, "cd") == 0) {
            str_copy(cwd, "/", sizeof(cwd));
        }
        else if (str_starts_with(p, "cd ")) {
            char* dir_arg = p + 3;
            while (*dir_arg == ' ') dir_arg++;

            if (*dir_arg == '\0' || str_cmp(dir_arg, "/") == 0) {
                str_copy(cwd, "/", sizeof(cwd));
            } else if (str_cmp(dir_arg, "..") == 0) {
                /* Sobe um diretorio */
                int len = str_len(cwd);
                if (len > 1) {
                    while (len > 1 && cwd[len - 1] != '/') {
                        len--;
                    }
                    if (len > 1 && cwd[len - 1] == '/') len--;
                    cwd[len] = '\0';
                }
            } else if (str_cmp(dir_arg, ".") == 0) {
                /* Mantem o mesmo diretorio */
            } else {
                char target[MAX_PATH_LEN];
                build_full_path(dir_arg, target, sizeof(target));
                int stat_res = sys_stat(target);
                if (stat_res == 1) {
                    str_copy(cwd, target, sizeof(cwd));
                } else if (stat_res == 0) {
                    sys_print("[CD] Erro: '");
                    sys_print(dir_arg);
                    sys_print("' e um arquivo, nao um diretorio.\n");
                } else {
                    sys_print("[CD] Erro: Diretorio '");
                    sys_print(dir_arg);
                    sys_print("' nao encontrado.\n");
                }
            }
        }
        else if (str_starts_with(p, "mkdir ")) {
            char* dir_arg = p + 6;
            while (*dir_arg == ' ') dir_arg++;
            if (*dir_arg == '\0') {
                sys_print("Uso: mkdir <nome_do_diretorio>\n");
            } else {
                char full_dir[MAX_PATH_LEN];
                build_full_path(dir_arg, full_dir, sizeof(full_dir));
                int res = sys_mkdir(full_dir);
                if (res == 0) {
                    sys_print("[VFS] Diretorio '");
                    sys_print(full_dir);
                    sys_print("' criado com sucesso.\n");
                } else {
                    sys_print("[VFS] Erro ao criar diretorio '");
                    sys_print(full_dir);
                    sys_print("'.\n");
                }
            }
        }
        else if (str_starts_with(p, "cat ")) {
            char* file_arg = p + 4;
            while (*file_arg == ' ') file_arg++;
            if (*file_arg == '\0') {
                sys_print("Uso: cat <caminho_do_arquivo>\n");
            } else {
                char full_file[MAX_PATH_LEN];
                build_full_path(file_arg, full_file, sizeof(full_file));
                int read_bytes = sys_read_file(full_file, file_buf, FILE_BUF_LEN - 1);
                if (read_bytes >= 0) {
                    file_buf[read_bytes] = '\0';
                    sys_print(file_buf);
                    if (read_bytes > 0 && file_buf[read_bytes - 1] != '\n') {
                        sys_print("\n");
                    }
                } else {
                    sys_print("[CAT] Erro: Arquivo '");
                    sys_print(file_arg);
                    sys_print("' nao encontrado ou nao e um arquivo legivel.\n");
                }
            }
        }
        else if (str_starts_with(p, "write ")) {
            char* args = p + 6;
            while (*args == ' ') args++;
            /* Separa nome do arquivo e texto */
            char fname[64];
            int f = 0;
            while (*args && *args != ' ' && f < 63) {
                fname[f++] = *args++;
            }
            fname[f] = '\0';
            while (*args == ' ') args++;

            if (f == 0 || *args == '\0') {
                sys_print("Uso: write <arquivo> <texto>\nExemplo: write teste.txt Ola Mundo!\n");
            } else {
                char full_file[MAX_PATH_LEN];
                build_full_path(fname, full_file, sizeof(full_file));
                int text_len = str_len(args);
                int written = sys_write_file(full_file, args, text_len);
                if (written >= 0) {
                    sys_print("[VFS] Gravados ");
                    print_num(written);
                    sys_print(" bytes em '");
                    sys_print(full_file);
                    sys_print("'.\n");
                } else {
                    sys_print("[VFS] Erro ao gravar no arquivo '");
                    sys_print(full_file);
                    sys_print("'.\n");
                }
            }
        }
        else if (str_starts_with(p, "notepad") || str_cmp(p, "notepad") == 0) {
            char exec_cmd[256];
            char* args = p + 7;
            while (*args == ' ') args++;

            if (*args == '\0') {
                str_copy(exec_cmd, "notepad", sizeof(exec_cmd));
            } else if (str_starts_with(args, "-editfile ") || str_starts_with(args, "-newfile ")) {
                int is_edit = str_starts_with(args, "-editfile ");
                char* farg = args + (is_edit ? 10 : 9);
                while (*farg == ' ') farg++;
                char full_f[MAX_PATH_LEN];
                build_full_path(farg, full_f, sizeof(full_f));

                str_copy(exec_cmd, is_edit ? "notepad -editfile " : "notepad -newfile ", sizeof(exec_cmd));
                int cur_l = str_len(exec_cmd);
                int k = 0;
                while (full_f[k] && cur_l < 254) {
                    exec_cmd[cur_l++] = full_f[k++];
                }
                exec_cmd[cur_l] = '\0';
            } else {
                char full_f[MAX_PATH_LEN];
                build_full_path(args, full_f, sizeof(full_f));
                str_copy(exec_cmd, "notepad ", sizeof(exec_cmd));
                int cur_l = str_len(exec_cmd);
                int k = 0;
                while (full_f[k] && cur_l < 254) {
                    exec_cmd[cur_l++] = full_f[k++];
                }
                exec_cmd[cur_l] = '\0';
            }

            sys_print("[SHELL] Abrindo editor /apps/notepad.bin em Ring 3...\n");
            int pid = sys_exec(exec_cmd, 1);
            if (pid < 0) {
                sys_print("[SHELL] Erro ao executar notepad.bin!\n");
            }
        }
        else if (str_starts_with(p, "downloadfile") || str_cmp(p, "downloadfile") == 0) {
            sys_print("[SHELL] Iniciando aplicativo /apps/downloadfile.bin em Ring 3...\n");
            int pid = sys_exec(p, 1);
            sys_print("[SHELL] Aplicativo downloadfile finalizado (PID ");
            print_num(pid);
            sys_print(").\n");
            if (pid < 0) {
                sys_print("[SHELL] Erro ao executar downloadfile.bin!\n");
            }
        }
        else if (str_cmp(p, "netinfo") == 0) {
            char ip[32] = {0};
            int has_ip = sys_net_info(ip);
            if (has_ip && ip[0] && str_cmp(ip, "0.0.0.0") != 0) {
                sys_print("[NET] Interface Intel e1000 ATIVA.\n");
                sys_print("[NET] Endereco IP: ");
                sys_print(ip);
                sys_print(" (Conectado a Internet via DHCP)\n");
            } else {
                sys_print("[NET] Interface desconectada ou DHCP aguardando resposta.\n");
            }
        }
        else if (str_cmp(p, "apps") == 0) {
            sys_print("[VFS] Executaveis e arquivos em /apps:\n");
            sys_list_dir("/apps");
        }
        else if (str_starts_with(p, "exec -app ")) {
            char* app_name = p + 10;
            while (*app_name == ' ') app_name++;
            if (*app_name == '\0') {
                sys_print("Uso: exec -app <nome_do_app>\nExemplo: exec -app hello\n");
            } else {
                sys_print("[SHELL] Invocando carregador VFS para '");
                sys_print(app_name);
                sys_print("'...\n");
                int pid = sys_exec(app_name, 1);
                if (pid < 0) {
                    sys_print("[SHELL] Erro ao executar o aplicativo '");
                    sys_print(app_name);
                    sys_print("'. Verifique se o arquivo existe via 'apps'.\n");
                } else {
                    sys_print("[SHELL] Aplicativo finalizado (PID ");
                    print_num(pid);
                    sys_print(").\n");
                }
            }
        }
        else if (str_cmp(p, "beep") == 0) {
            sys_print("[DRIVER] Emitindo tom sonoro via PC Speaker (Canal 2 PIT)...\n");
            sys_speaker(750, 150);
            sys_print("[DRIVER] Beep concluido!\n");
        }
        else if (str_cmp(p, "nvram") == 0) {
            sys_print("[DRIVER] Testando leitura do driver NVRAM/CMOS:\n");
            unsigned char sec = sys_nvram_read(0x00);
            unsigned char min = sys_nvram_read(0x02);
            unsigned char hour = sys_nvram_read(0x04);
            sys_print("  RTC CMOS Horario: ");
            print_num((int)hour);
            sys_print(":");
            print_num((int)min);
            sys_print(":");
            print_num((int)sec);
            sys_print(" (Lido com sucesso via porta 0x70/0x71)\n");
        }
        else if (str_starts_with(p, "echo ")) {
            sys_print(p + 5);
            sys_print("\n");
        }
        else if (str_cmp(p, "clear") == 0) {
            sys_clear();
        }
        else if (str_cmp(p, "exit") == 0) {
            sys_print("[SHELL] Encerrando shell de usuario...\n");
            sys_exit(0);
        }
        else {
            sys_print("Comando desconhecido: '");
            sys_print(p);
            sys_print("'. Digite 'help' para listar os comandos.\n");
        }
    }

    return 0;
}
