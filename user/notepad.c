#include "include/syscall.h"

#define MAX_LINES       256
#define MAX_LINE_LEN    100
#define RAW_BUF_SIZE    32768

#define COLOR_TITLE     0x70  /* Fundo Cinza Claro, Texto Preto */
#define COLOR_EDITOR    0x1F  /* Fundo Azul Escuro, Texto Branco Brilhante (Estilo MS-DOS Edit) */
#define COLOR_STATUS    0x70  /* Fundo Cinza Claro, Texto Preto */

static char lines[MAX_LINES][MAX_LINE_LEN];
static int  line_count = 1;
static int  cur_line   = 0;
static int  cur_col    = 0;
static int  top_line   = 0;

static char filepath[128];
static char raw_buffer[RAW_BUF_SIZE];

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

static void int_to_str(int val, char* buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char tmp[16];
    int t = 0;
    int v = val < 0 ? -val : val;
    while (v > 0) {
        tmp[t++] = '0' + (v % 10);
        v /= 10;
    }
    if (val < 0) tmp[t++] = '-';
    int b = 0;
    while (t > 0) {
        buf[b++] = tmp[--t];
    }
    buf[b] = '\0';
}

/* Desenha a interface no estilo MS-DOS Edit */
static void render_screen(void) {
    /* 1. Barra de Titulo Superior (Linha 0) */
    char title_bar[81];
    for (int i = 0; i < 80; i++) title_bar[i] = ' ';
    title_bar[80] = '\0';

    const char* prefix = " [Vectra Notepad Editor]  Arquivo: ";
    int p_len = str_len(prefix);
    for (int i = 0; i < p_len && i < 80; i++) title_bar[i] = prefix[i];

    int f_len = str_len(filepath);
    for (int i = 0; i < f_len && (p_len + i) < 55; i++) {
        title_bar[p_len + i] = filepath[i];
    }

    const char* esc_help = "[ESC] Salvar e Sair";
    int e_len = str_len(esc_help);
    for (int i = 0; i < e_len; i++) {
        title_bar[80 - e_len - 2 + i] = esc_help[i];
    }
    sys_console_draw_row(0, title_bar, COLOR_TITLE);

    /* 2. Area do Editor de Texto (Linhas 1 a 23 com Fundo Azul MS-DOS) */
    for (int row = 1; row <= 23; row++) {
        int l_idx = top_line + (row - 1);
        if (l_idx < line_count) {
            sys_console_draw_row(row, lines[l_idx], COLOR_EDITOR);
        } else {
            sys_console_draw_row(row, "", COLOR_EDITOR);
        }
    }

    /* 3. Barra de Status Inferior (Linha 24) */
    char status_bar[81];
    for (int i = 0; i < 80; i++) status_bar[i] = ' ';
    status_bar[80] = '\0';

    char l_str[16], c_str[16], tot_str[16];
    int_to_str(cur_line + 1, l_str);
    int_to_str(cur_col + 1, c_str);
    int_to_str(line_count, tot_str);

    char pos_info[64];
    int pos_len = 0;
    #define APPEND_STATUS(str) do { \
        const char* _s = (str); \
        while (*_s && pos_len < 60) pos_info[pos_len++] = *_s++; \
    } while(0)

    APPEND_STATUS(" Linha: ");
    APPEND_STATUS(l_str);
    APPEND_STATUS("/");
    APPEND_STATUS(tot_str);
    APPEND_STATUS("  Col: ");
    APPEND_STATUS(c_str);
    APPEND_STATUS(" | [Enter] Nova Linha  [Setas] Navegar  [ESC] Salvar");
    pos_info[pos_len] = '\0';

    for (int i = 0; i < pos_len && i < 80; i++) {
        status_bar[i] = pos_info[i];
    }
    sys_console_draw_row(24, status_bar, COLOR_STATUS);

    /* 4. Posiciona o cursor de hardware na posicao correspondente */
    int screen_row = 1 + (cur_line - top_line);
    int screen_col = cur_col;
    if (screen_col >= 80) screen_col = 79;
    if (screen_row >= 1 && screen_row <= 23) {
        sys_console_set_cursor(screen_col, screen_row);
    }
}

static void adjust_scroll(void) {
    if (cur_line < top_line) {
        top_line = cur_line;
    }
    if (cur_line >= top_line + 23) {
        top_line = cur_line - 22;
    }
    if (top_line < 0) top_line = 0;
}

/* Insere caractere na posicao atual */
static void insert_char(char c) {
    int len = str_len(lines[cur_line]);
    if (len >= MAX_LINE_LEN - 2) return; /* Limite de largura da linha */

    for (int i = len; i >= cur_col; i--) {
        lines[cur_line][i + 1] = lines[cur_line][i];
    }
    lines[cur_line][cur_col] = c;
    cur_col++;
}

/* Quebra a linha atual (Enter) */
static void insert_newline(void) {
    if (line_count >= MAX_LINES - 1) return;

    /* Move todas as linhas seguintes para baixo */
    for (int i = line_count; i > cur_line + 1; i--) {
        str_copy(lines[i], lines[i - 1], MAX_LINE_LEN);
    }

    /* Copia os caracteres apos o cursor para a nova linha */
    int len = str_len(lines[cur_line]);
    int new_col = 0;
    for (int i = cur_col; i < len; i++) {
        lines[cur_line + 1][new_col++] = lines[cur_line][i];
    }
    lines[cur_line + 1][new_col] = '\0';
    lines[cur_line][cur_col] = '\0';

    line_count++;
    cur_line++;
    cur_col = 0;
}

/* Apaga caractere anterior (Backspace) */
static void delete_backspace(void) {
    if (cur_col > 0) {
        int len = str_len(lines[cur_line]);
        for (int i = cur_col - 1; i < len; i++) {
            lines[cur_line][i] = lines[cur_line][i + 1];
        }
        cur_col--;
    } else if (cur_line > 0) {
        /* Concatena linha atual ao final da linha anterior */
        int prev_len = str_len(lines[cur_line - 1]);
        int cur_len = str_len(lines[cur_line]);

        if (prev_len + cur_len < MAX_LINE_LEN - 1) {
            for (int i = 0; i < cur_len; i++) {
                lines[cur_line - 1][prev_len + i] = lines[cur_line][i];
            }
            lines[cur_line - 1][prev_len + cur_len] = '\0';

            /* Remove a linha atual puxando as seguintes */
            for (int i = cur_line; i < line_count - 1; i++) {
                str_copy(lines[i], lines[i + 1], MAX_LINE_LEN);
            }
            line_count--;
            cur_line--;
            cur_col = prev_len;
        }
    }
}

/* Apaga caractere sob o cursor (Delete) */
static void delete_forward(void) {
    int len = str_len(lines[cur_line]);
    if (cur_col < len) {
        for (int i = cur_col; i < len; i++) {
            lines[cur_line][i] = lines[cur_line][i + 1];
        }
    } else if (cur_line < line_count - 1) {
        int next_len = str_len(lines[cur_line + 1]);
        if (len + next_len < MAX_LINE_LEN - 1) {
            for (int i = 0; i < next_len; i++) {
                lines[cur_line][len + i] = lines[cur_line + 1][i];
            }
            lines[cur_line][len + next_len] = '\0';

            for (int i = cur_line + 1; i < line_count - 1; i++) {
                str_copy(lines[i], lines[i + 1], MAX_LINE_LEN);
            }
            line_count--;
        }
    }
}

/* Salva todo o conteudo no VFS e finaliza o editor */
static void save_and_exit(void) {
    int total_bytes = 0;
    for (int l = 0; l < line_count; l++) {
        int len = str_len(lines[l]);
        for (int i = 0; i < len && total_bytes < RAW_BUF_SIZE - 2; i++) {
            raw_buffer[total_bytes++] = lines[l][i];
        }
        if (l < line_count - 1 && total_bytes < RAW_BUF_SIZE - 2) {
            raw_buffer[total_bytes++] = '\n';
        }
    }
    raw_buffer[total_bytes] = '\0';

    int res = sys_write_file(filepath, raw_buffer, total_bytes);

    sys_clear();
    sys_print("====================================================\n");
    sys_print("  VECTRA OS - NOTEPAD (EDITOR MS-DOS STYLE)\n");
    sys_print("====================================================\n");
    if (res >= 0) {
        sys_print("[NOTEPAD] Sucesso: Arquivo '");
        sys_print(filepath);
        sys_print("' gravado permanentemente no VFS!\n");
        sys_print("[NOTEPAD] Total de linhas: ");
        char buf[16];
        int_to_str(line_count, buf);
        sys_print(buf);
        sys_print(" | Bytes: ");
        int_to_str(total_bytes, buf);
        sys_print(buf);
        sys_print(".\n");
    } else {
        sys_print("[NOTEPAD] Erro ao gravar o arquivo '");
        sys_print(filepath);
        sys_print("' no VFS!\n");
    }
    sys_print("----------------------------------------------------\n\n");

    sys_exit(0);
}

int main(void) {
    char cmdline[256];
    sys_get_cmdline(cmdline, sizeof(cmdline));

    /* Analisa os argumentos: notepad -newfile <arq> ou notepad -editfile <arq> */
    int is_edit = -1;
    char target_file[128] = {0};

    char* p = cmdline;
    while (*p && *p != ' ') p++; /* Pula o nome do executavel */
    while (*p == ' ') p++;

    if (str_starts_with(p, "-editfile ") || str_starts_with(p, "-editfile")) {
        is_edit = 1;
        p += 9;
        while (*p == ' ') p++;
    } else if (str_starts_with(p, "-newfile ") || str_starts_with(p, "-newfile")) {
        is_edit = 0;
        p += 8;
        while (*p == ' ') p++;
    } else if (*p != '\0') {
        /* Se informou direto o nome do arquivo, detecta se existe */
        is_edit = -1;
    }

    int t = 0;
    while (*p && *p != ' ' && t < 126) {
        target_file[t++] = *p++;
    }
    target_file[t] = '\0';

    if (target_file[0] == '\0') {
        sys_print("Uso do Notepad:\n");
        sys_print("  notepad -newfile <arquivo>   - Cria um novo arquivo para edicao\n");
        sys_print("  notepad -editfile <arquivo>  - Abre um arquivo existente para editar\n");
        sys_print("Exemplos:\n");
        sys_print("  notepad -newfile /meutexto.txt\n");
        sys_print("  notepad -editfile /docs/readme.txt\n");
        sys_exit(1);
        return 1;
    }

    /* Se o caminho nao comecar com '/', garante caminho absoluto canonico */
    if (target_file[0] != '/') {
        filepath[0] = '/';
        str_copy(&filepath[1], target_file, sizeof(filepath) - 1);
    } else {
        str_copy(filepath, target_file, sizeof(filepath));
    }

    /* Inicializa as linhas */
    for (int l = 0; l < MAX_LINES; l++) {
        lines[l][0] = '\0';
    }
    line_count = 1;

    /* Se for edicao ou autodeteccao */
    int file_stat = sys_stat(filepath);
    if (is_edit == 1 && file_stat != 0) {
        sys_print("[NOTEPAD] Erro: O arquivo '");
        sys_print(filepath);
        sys_print("' nao existe para edicao. Use -newfile para cria-lo.\n");
        sys_exit(1);
        return 1;
    }

    if (is_edit == 1 || (is_edit == -1 && file_stat == 0)) {
        /* Le o conteudo do arquivo existente de forma identica */
        int bytes = sys_read_file(filepath, raw_buffer, sizeof(raw_buffer) - 1);
        if (bytes > 0) {
            raw_buffer[bytes] = '\0';
            line_count = 0;
            int c = 0;
            for (int i = 0; i < bytes; i++) {
                char ch = raw_buffer[i];
                if (ch == '\r') continue;
                if (ch == '\n') {
                    lines[line_count][c] = '\0';
                    line_count++;
                    c = 0;
                    if (line_count >= MAX_LINES) break;
                } else {
                    if (c < MAX_LINE_LEN - 1) {
                        lines[line_count][c++] = ch;
                    }
                }
            }
            if (line_count < MAX_LINES) {
                lines[line_count][c] = '\0';
                line_count++;
            }
            if (line_count == 0) line_count = 1;
        }
    }

    cur_line = 0;
    cur_col = 0;
    top_line = 0;

    /* Loop Principal do Editor MSDOS */
    for (;;) {
        adjust_scroll();
        render_screen();

        int ch = sys_getchar(1); /* Leitura bloqueante */

        if (ch == 27) {
            /* Tecla ESC: Salva e sai */
            save_and_exit();
            break;
        } else if (ch == '\n' || ch == '\r') {
            /* Enter: Nova linha multiline */
            insert_newline();
        } else if (ch == '\b' || ch == 127) {
            /* Backspace */
            delete_backspace();
        } else if (ch == '\t') {
            /* Tab: insere 4 espacos */
            for (int k = 0; k < 4; k++) insert_char(' ');
        } else if (ch == KEY_UP) {
            if (cur_line > 0) cur_line--;
            int len = str_len(lines[cur_line]);
            if (cur_col > len) cur_col = len;
        } else if (ch == KEY_DOWN) {
            if (cur_line < line_count - 1) cur_line++;
            int len = str_len(lines[cur_line]);
            if (cur_col > len) cur_col = len;
        } else if (ch == KEY_LEFT) {
            if (cur_col > 0) {
                cur_col--;
            } else if (cur_line > 0) {
                cur_line--;
                cur_col = str_len(lines[cur_line]);
            }
        } else if (ch == KEY_RIGHT) {
            int len = str_len(lines[cur_line]);
            if (cur_col < len) {
                cur_col++;
            } else if (cur_line < line_count - 1) {
                cur_line++;
                cur_col = 0;
            }
        } else if (ch == KEY_HOME) {
            cur_col = 0;
        } else if (ch == KEY_END) {
            cur_col = str_len(lines[cur_line]);
        } else if (ch == KEY_DC) {
            delete_forward();
        } else if (ch >= 32 && ch <= 126) {
            /* Caracteres alfanumericos e pontuacao normais */
            insert_char((char)ch);
        }
    }

    return 0;
}
