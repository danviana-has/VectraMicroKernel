#ifndef VECTRA_HTTP_CLIENT_H
#define VECTRA_HTTP_CLIENT_H

#include "types.h"

#define HTTP_OK                   0
#define HTTP_ERR_INVALID_URL     -1
#define HTTP_ERR_NO_NETWORK      -2
#define HTTP_ERR_DNS             -3
#define HTTP_ERR_CONNECT         -4
#define HTTP_ERR_SEND            -5
#define HTTP_ERR_RECV            -6
#define HTTP_ERR_VFS             -7
#define HTTP_ERR_TIMEOUT         -8

/*
 * Baixa um arquivo a partir de uma URL web real (http:// ou https://)
 * e o salva diretamente no VFS em dest_path (ex: /downloads/arquivo.txt).
 * Se dest_path for NULL ou "", salva automaticamente em /downloads/<nome_do_arquivo>.
 * Retorna o total de bytes baixados (> 0) em caso de sucesso, ou codigo de erro (< 0).
 */
int http_download(const char* url, const char* dest_path);

#endif /* VECTRA_HTTP_CLIENT_H */
