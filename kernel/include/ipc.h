#ifndef VECTRA_IPC_H
#define VECTRA_IPC_H

#include "types.h"

#define IPC_OK        0
#define IPC_ERR      -1
#define IPC_WOULDBLOCK -2

#define IPC_ANY_SENDER 0xFFFFFFFF

/* Estrutura padronizada de mensagem do Vectra OS */
typedef struct {
    uint32_t sender;   /* PID da tarefa remetente */
    uint32_t type;     /* Tipo/Comando da mensagem */
    uint32_t data[4];  /* Carga util de dados direta */
} message_t;

int32_t ipc_send(uint32_t dest_pid, message_t* msg);
int32_t ipc_receive(uint32_t from_pid, message_t* msg);
int32_t ipc_reply(uint32_t dest_pid, message_t* msg);

#endif /* VECTRA_IPC_H */
