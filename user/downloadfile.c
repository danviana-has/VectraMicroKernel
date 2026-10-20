#include "include/syscall.h"

static int str_cmp(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] && s2[i]) {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        i++;
    }
    return s1[i] - s2[i];
}

static int str_starts_with(const char* str, const char* prefix) {
    int i = 0;
    while (prefix[i]) {
        if (str[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}

static void print_num(int num) {
    if (num == 0) {
        sys_print("0");
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
        int ch = sys_getchar(1);
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
            char echo[2] = { (char)ch, '\0' };
            sys_print(echo);
        }
    }
    buffer[idx] = '\0';
}

int main(void) {
    sys_print("====================================================\n");
    sys_print("  Vectra OS - Web File Downloader (Ring 3 App)     \n");
    sys_print("  Suporte a download HTTP / HTTPS direto no VFS    \n");
    sys_print("====================================================\n");

    /* 1. Verifica status da rede */
    char ip_str[32] = {0};
    int has_ip = sys_net_info(ip_str);
    if (!has_ip || ip_str[0] == '\0' || str_cmp(ip_str, "0.0.0.0") == 0) {
        sys_print("[DOWNLOAD] Aviso: Interface de rede ainda obtendo IP via DHCP...\n");
        sys_print("[DOWNLOAD] Tentando aguardar ativacao da rede...\n");
        for (int w = 0; w < 50; w++) {
            sys_yield();
            has_ip = sys_net_info(ip_str);
            if (has_ip && ip_str[0] != '\0' && str_cmp(ip_str, "0.0.0.0") != 0) break;
        }
    }

    if (has_ip && ip_str[0] != '\0') {
        sys_print("[DOWNLOAD] Rede conectada. IP local: ");
        sys_print(ip_str);
        sys_print("\n");
    }

    /* 2. Recupera a linha de comando */
    char cmdline[256] = {0};
    sys_get_cmdline(cmdline, sizeof(cmdline));

    char url[256] = {0};
    char dest_path[128] = {0};

    /* Procura flag -web */
    const char* web_flag = "-web ";
    char* pos = NULL;
    for (int i = 0; cmdline[i]; i++) {
        if (str_starts_with(&cmdline[i], web_flag)) {
            pos = &cmdline[i + 5];
            break;
        }
    }

    if (pos) {
        while (*pos == ' ') pos++;
        /* Extrai a URL */
        int u = 0;
        while (*pos && *pos != ' ' && u < 254) {
            url[u++] = *pos++;
        }
        url[u] = '\0';

        /* Se houver argumento seguinte, e o destino customizado */
        while (*pos == ' ') pos++;
        if (*pos) {
            int d = 0;
            while (*pos && *pos != ' ' && d < 126) {
                dest_path[d++] = *pos++;
            }
            dest_path[d] = '\0';
        }
    }

    /* Se nao informou URL pela linha de comando, solicita interativamente */
    if (url[0] == '\0') {
        sys_print("[DOWNLOAD] Informe a URL do arquivo para download:\n");
        sys_print("URL: ");
        read_line(url, sizeof(url));
    }

    if (url[0] == '\0') {
        sys_print("[DOWNLOAD] Erro: Nenhuma URL fornecida. Operacao cancelada.\n");
        sys_exit(1);
        return 1;
    }

    sys_print("[DOWNLOAD] Baixando a partir de: ");
    sys_print(url);
    sys_print("\n");

    /* 3. Invoca a chamada de sistema para download */
    int bytes = sys_download_file(url, dest_path[0] ? dest_path : "/downloads/");

    if (bytes > 0) {
        sys_print("----------------------------------------------------\n");
        sys_print("[DOWNLOAD] SUCESSO TOTAL!\n");
        sys_print("[DOWNLOAD] Total de bytes transferidos: ");
        print_num(bytes);
        sys_print(" bytes.\n");
        sys_print("[DOWNLOAD] O arquivo foi salvo permanentemente no VFS.\n");
        sys_print("[DOWNLOAD] Use 'ls /downloads' para listar ou 'cat <caminho>' para ver o conteudo.\n");
        sys_print("----------------------------------------------------\n");
    } else {
        sys_print("[DOWNLOAD] FALHA: Nao foi possivel concluir o download (Codigo de erro: ");
        print_num(bytes);
        sys_print(").\n");
    }

    sys_exit(0);
    return 0;
}
