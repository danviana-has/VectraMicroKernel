#include "http_client.h"
#include "drivers.h"
#include "vfs.h"
#include "heap.h"
#include "timer.h"
#include "task.h"
#include "netif_e1000.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include <string.h>
#include <stdlib.h>

#define HTTP_MAX_BUFFER_SIZE  (512 * 1024) /* Ate 512 KiB por download */
#define HTTP_TIMEOUT_MS       15000         /* 15 segundos de timeout maximo */

typedef enum {
    HTTP_STATE_IDLE,
    HTTP_STATE_DNS_WAIT,
    HTTP_STATE_CONNECTING,
    HTTP_STATE_CONNECTED,
    HTTP_STATE_RECEIVING,
    HTTP_STATE_DONE,
    HTTP_STATE_ERROR
} http_client_state_t;

typedef struct {
    char url[256];
    char host[128];
    char path[256];
    char filename[64];
    char dest_vfs_path[128];
    uint16_t port;
    bool is_https;

    ip_addr_t server_ip;
    struct tcp_pcb* pcb;

    volatile http_client_state_t state;
    int error_code;
    int http_response_code;

    /* Buffer dinamico para acumular os dados do corpo (body) */
    uint8_t* rx_buffer;
    uint32_t rx_capacity;
    uint32_t rx_len;

    /* Parser de cabecalhos HTTP */
    bool headers_done;
    uint32_t header_bytes_accumulated;
    char header_buf[2048];

    uint32_t start_tick;
} http_client_session_t;

static http_client_session_t session;

/* Parser robusto de URL */
static bool parse_url(const char* url, bool* is_https, char* host, uint16_t* port, char* path, char* filename) {
    if (!url || !*url) return false;

    const char* p = url;
    *is_https = false;
    *port = 80;

    if (strncmp(p, "http://", 7) == 0) {
        p += 7;
        *port = 80;
        *is_https = false;
    } else if (strncmp(p, "https://", 8) == 0) {
        p += 8;
        *port = 443;
        *is_https = true;
    }

    /* Extrai o Host e porta opcional */
    int i = 0;
    while (*p && *p != '/' && *p != ':' && i < 127) {
        host[i++] = *p++;
    }
    host[i] = '\0';
    if (i == 0) return false;

    /* Porta explicita (ex: :8080) */
    if (*p == ':') {
        p++;
        int custom_port = 0;
        while (*p >= '0' && *p <= '9') {
            custom_port = custom_port * 10 + (*p - '0');
            p++;
        }
        if (custom_port > 0 && custom_port <= 65535) {
            *port = (uint16_t)custom_port;
        }
    }

    /* Extrai o Path */
    if (*p == '/') {
        int j = 0;
        while (*p && j < 254) {
            path[j++] = *p++;
        }
        path[j] = '\0';
    } else {
        path[0] = '/';
        path[1] = '\0';
    }

    /* Extrai o nome do arquivo a partir do path */
    const char* last_slash = strchr(path, '/');
    const char* cur = path;
    while ((cur = strchr(cur, '/')) != NULL) {
        last_slash = cur;
        cur++;
    }

    if (last_slash && *(last_slash + 1) != '\0') {
        int k = 0;
        const char* f = last_slash + 1;
        while (*f && *f != '?' && *f != '#' && k < 63) {
            filename[k++] = *f++;
        }
        filename[k] = '\0';
    } else {
        strcpy(filename, "index.html");
    }

    return true;
}

/* Callback de resolucao de DNS do lwIP */
static void http_dns_callback(const char* name, const ip_addr_t* ipaddr, void* arg) {
    (void)name;
    http_client_session_t* s = (http_client_session_t*)arg;
    if (ipaddr != NULL) {
        s->server_ip = *ipaddr;
        s->state = HTTP_STATE_CONNECTING;
    } else {
        s->error_code = HTTP_ERR_DNS;
        s->state = HTTP_STATE_ERROR;
    }
}

/* Callback de erro TCP */
static void http_tcp_err_callback(void* arg, err_t err) {
    (void)err;
    http_client_session_t* s = (http_client_session_t*)arg;
    s->pcb = NULL;
    if (s->state != HTTP_STATE_DONE) {
        s->error_code = HTTP_ERR_CONNECT;
        s->state = HTTP_STATE_ERROR;
    }
}

/* Callback de conexao TCP bem-sucedida */
static err_t http_tcp_connected_callback(void* arg, struct tcp_pcb* tpcb, err_t err) {
    http_client_session_t* s = (http_client_session_t*)arg;
    if (err != ERR_OK) {
        s->error_code = HTTP_ERR_CONNECT;
        s->state = HTTP_STATE_ERROR;
        return err;
    }

    s->state = HTTP_STATE_CONNECTED;

    /* Monta a requisicao HTTP/1.1 padrao */
    char req[512];
    int len = 0;

    #define APPEND_STR(str) do { \
        const char* _src = (str); \
        while (*_src && len < 500) req[len++] = *_src++; \
    } while(0)

    APPEND_STR("GET ");
    APPEND_STR(s->path);
    APPEND_STR(" HTTP/1.1\r\nHost: ");
    APPEND_STR(s->host);
    APPEND_STR("\r\nUser-Agent: VectraOS/1.0 (x86 Bare-Metal)\r\nAccept: */*\r\nConnection: close\r\n\r\n");
    req[len] = '\0';

    kprint("[HTTP] Enviando requisicao: GET ");
    kprint(s->path);
    kprint(" para ");
    kprint(s->host);
    kprint("...\n");

    err_t write_err = tcp_write(tpcb, req, len, TCP_WRITE_FLAG_COPY);
    if (write_err != ERR_OK) {
        s->error_code = HTTP_ERR_SEND;
        s->state = HTTP_STATE_ERROR;
        return write_err;
    }

    tcp_output(tpcb);
    s->state = HTTP_STATE_RECEIVING;
    return ERR_OK;
}

/* Callback de recepcao de dados TCP */
static err_t http_tcp_recv_callback(void* arg, struct tcp_pcb* tpcb, struct pbuf* p, err_t err) {
    http_client_session_t* s = (http_client_session_t*)arg;

    if (err != ERR_OK) {
        if (p) pbuf_free(p);
        s->error_code = HTTP_ERR_RECV;
        s->state = HTTP_STATE_ERROR;
        return err;
    }

    /* Servidor fechou a conexao (EOF - Download completo) */
    if (p == NULL) {
        kprint("[HTTP] Conexao encerrada pelo servidor remoto (EOF).\n");
        s->state = HTTP_STATE_DONE;
        tcp_close(tpcb);
        s->pcb = NULL;
        return ERR_OK;
    }

    /* Concatena os bytes do pbuf recebido */
    struct pbuf* q = p;
    while (q != NULL) {
        const uint8_t* payload = (const uint8_t*)q->payload;
        uint16_t plen = q->len;

        if (!s->headers_done) {
            /* Acumula bytes no buffer de cabecalho para procurar \r\n\r\n */
            uint16_t i = 0;
            while (i < plen && !s->headers_done) {
                if (s->header_bytes_accumulated < sizeof(s->header_buf) - 1) {
                    s->header_buf[s->header_bytes_accumulated++] = payload[i];
                    s->header_buf[s->header_bytes_accumulated] = '\0';
                }

                /* Procura delimitador de fim de cabecalhos HTTP \r\n\r\n ou \n\n */
                if (s->header_bytes_accumulated >= 4) {
                    char* end = strstr(s->header_buf, "\r\n\r\n");
                    int delim_len = 4;
                    if (!end) {
                        end = strstr(s->header_buf, "\n\n");
                        delim_len = 2;
                    }

                    if (end) {
                        s->headers_done = true;

                        /* Extrai o codigo de status HTTP (ex: HTTP/1.1 200 OK) */
                        char* space = strchr(s->header_buf, ' ');
                        if (space) {
                            s->http_response_code = atoi(space + 1);
                        } else {
                            s->http_response_code = 200;
                        }

                        kprint("[HTTP] Resposta do servidor recebida: Status ");
                        kprint_dec(s->http_response_code);
                        kprint("\n");

                        /* O restante dos bytes neste pacote pertence ao corpo do arquivo */
                        uint16_t body_start_offset = (uint16_t)((end - s->header_buf) + delim_len);
                        uint32_t header_consumed_in_chunk = s->header_bytes_accumulated - body_start_offset;

                        if (header_consumed_in_chunk > 0 && s->rx_buffer) {
                            const uint8_t* body_src = (const uint8_t*)(end + delim_len);
                            for (uint32_t b = 0; b < header_consumed_in_chunk && s->rx_len < s->rx_capacity; b++) {
                                s->rx_buffer[s->rx_len++] = body_src[b];
                            }
                        }
                        i++;
                        break;
                    }
                }
                i++;
            }

            /* Se ja passou dos cabecalhos, copia o restante dos bytes deste pbuf */
            if (s->headers_done && i < plen && s->rx_buffer) {
                while (i < plen && s->rx_len < s->rx_capacity) {
                    s->rx_buffer[s->rx_len++] = payload[i++];
                }
            }
        } else {
            /* Cabecalhos ja processados: copia diretamente para o rx_buffer */
            if (s->rx_buffer) {
                for (uint16_t i = 0; i < plen && s->rx_len < s->rx_capacity; i++) {
                    s->rx_buffer[s->rx_len++] = payload[i];
                }
            }
        }

        q = q->next;
    }

    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

int http_download(const char* url, const char* dest_path) {
    if (!url || !*url) return HTTP_ERR_INVALID_URL;

    /* Verifica conectividade de rede */
    if (!net_has_ip()) {
        kprint("[HTTP] Erro: Interface de rede ainda nao possui IP (DHCP ativo).\n");
        return HTTP_ERR_NO_NETWORK;
    }

    /* Limpa e inicializa a sessao */
    memset(&session, 0, sizeof(session));
    strcpy(session.url, url);

    if (!parse_url(url, &session.is_https, session.host, &session.port, session.path, session.filename)) {
        kprint("[HTTP] Erro: Formato de URL invalido!\n");
        return HTTP_ERR_INVALID_URL;
    }

    /* Define o caminho de destino no VFS */
    if (dest_path && *dest_path) {
        /* Se dest_path termina com '/', junta com session.filename */
        size_t dlen = strlen(dest_path);
        if (dest_path[dlen - 1] == '/') {
            strcpy(session.dest_vfs_path, dest_path);
            strcat(session.dest_vfs_path, session.filename);
        } else {
            strcpy(session.dest_vfs_path, dest_path);
        }
    } else {
        strcpy(session.dest_vfs_path, "/downloads/");
        strcat(session.dest_vfs_path, session.filename);
    }

    /* Aloca buffer temporario de recepcao na Heap */
    session.rx_capacity = HTTP_MAX_BUFFER_SIZE;
    session.rx_buffer = (uint8_t*)kmalloc(session.rx_capacity);
    if (!session.rx_buffer) {
        kprint("[HTTP] Erro: Memoria insuficiente para buffer de download.\n");
        return HTTP_ERR_VFS;
    }
    session.rx_len = 0;

    kprint("[HTTP] Conectando a: ");
    kprint(session.is_https ? "https://" : "http://");
    kprint(session.host);
    kprint(":");
    kprint_dec(session.port);
    kprint(session.path);
    kprint("\n[HTTP] Destino VFS: ");
    kprint(session.dest_vfs_path);
    kprint("\n");

    /* 1. Resolucao DNS */
    session.state = HTTP_STATE_DNS_WAIT;
    session.start_tick = timer_get_ticks();

    if (ip4addr_aton(session.host, &session.server_ip)) {
        /* Host e um IP numerico direto */
        session.state = HTTP_STATE_CONNECTING;
    } else {
        kprint("[HTTP] Resolvendo DNS para '");
        kprint(session.host);
        kprint("'...\n");

        err_t dns_err = dns_gethostbyname(session.host, &session.server_ip, http_dns_callback, &session);
        if (dns_err == ERR_OK) {
            session.state = HTTP_STATE_CONNECTING;
        } else if (dns_err != ERR_INPROGRESS) {
            kprint("[HTTP] Falha imediata ao resolver DNS.\n");
            kfree(session.rx_buffer);
            return HTTP_ERR_DNS;
        }
    }

    /* Aguarda resolucao de DNS */
    while (session.state == HTTP_STATE_DNS_WAIT) {
        net_poll();
        task_yield();
        if ((timer_get_ticks() - session.start_tick) * 10 > HTTP_TIMEOUT_MS) {
            kprint("[HTTP] Timeout aguardando resolucao DNS.\n");
            kfree(session.rx_buffer);
            return HTTP_ERR_TIMEOUT;
        }
    }

    if (session.state == HTTP_STATE_ERROR) {
        kprint("[HTTP] Erro: Nao foi possivel resolver o dominio informado.\n");
        kfree(session.rx_buffer);
        return HTTP_ERR_DNS;
    }

    /* 2. Estabelece Conexao TCP */
    session.pcb = tcp_new();
    if (!session.pcb) {
        kprint("[HTTP] Erro: Nao foi possivel criar socket TCP lwIP.\n");
        kfree(session.rx_buffer);
        return HTTP_ERR_CONNECT;
    }

    tcp_arg(session.pcb, &session);
    tcp_err(session.pcb, http_tcp_err_callback);
    tcp_recv(session.pcb, http_tcp_recv_callback);

    session.state = HTTP_STATE_CONNECTING;
    session.start_tick = timer_get_ticks();

    err_t conn_err = tcp_connect(session.pcb, &session.server_ip, session.port, http_tcp_connected_callback);
    if (conn_err != ERR_OK) {
        kprint("[HTTP] Erro ao disparar conexao TCP.\n");
        tcp_abort(session.pcb);
        kfree(session.rx_buffer);
        return HTTP_ERR_CONNECT;
    }

    /* 3. Loop de Polling e Transferencia */
    while (session.state != HTTP_STATE_DONE && session.state != HTTP_STATE_ERROR) {
        net_poll();
        task_yield();

        /* Timeout de inatividade */
        if ((timer_get_ticks() - session.start_tick) * 10 > HTTP_TIMEOUT_MS) {
            kprint("[HTTP] Timeout durante a conexao/download.\n");
            if (session.pcb) tcp_abort(session.pcb);
            kfree(session.rx_buffer);
            return HTTP_ERR_TIMEOUT;
        }
    }

    if (session.state == HTTP_STATE_ERROR) {
        kprint("[HTTP] Erro durante a transferencia de dados.\n");
        if (session.pcb) tcp_abort(session.pcb);
        kfree(session.rx_buffer);
        return session.error_code;
    }

    /* 4. Grava o arquivo baixado no VFS Real */
    kprint("[HTTP] Download finalizado! Gravando ");
    kprint_dec(session.rx_len);
    kprint(" bytes no VFS em '");
    kprint(session.dest_vfs_path);
    kprint("'...\n");

    vfs_node_t* vfs_file = vfs_create_file(session.dest_vfs_path, session.rx_buffer, session.rx_len);
    kfree(session.rx_buffer);

    if (!vfs_file) {
        kprint("[HTTP] Erro ao salvar arquivo no sistema de arquivos VFS!\n");
        return HTTP_ERR_VFS;
    }

    kprint("[HTTP] Sucesso: Arquivo salvo no VFS!\n");
    return (int)session.rx_len;
}
