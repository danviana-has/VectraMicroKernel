#include "ipc.h"
#include "task.h"

int32_t ipc_send(uint32_t dest_pid, message_t* msg) {
    if (!msg) return IPC_ERR;

    task_t* current = get_current_task();
    task_t* dest    = get_task_by_id(dest_pid);

    if (!dest || dest == current) return IPC_ERR;

    msg->sender = current->id;

    /* 1. Rendezvous: o destino ja esta bloqueado esperando mensagem? */
    if (dest->state == TASK_BLOCKED_RECEIVE &&
       (dest->ipc_target_pid == current->id || dest->ipc_target_pid == IPC_ANY_SENDER)) {

        /* Copia os dados diretamente no buffer do destinatario */
        if (dest->ipc_buf) {
            dest->ipc_buf->sender  = msg->sender;
            dest->ipc_buf->type    = msg->type;
            dest->ipc_buf->data[0] = msg->data[0];
            dest->ipc_buf->data[1] = msg->data[1];
            dest->ipc_buf->data[2] = msg->data[2];
            dest->ipc_buf->data[3] = msg->data[3];
        }

        /* Desbloqueia a tarefa destino para que ela possa processar */
        dest->state = TASK_READY;
        return IPC_OK;
    }

    /* 2. Caso o destino ainda nao esteja pronto, o remetente bloqueia (Sincrono) */
    current->ipc_buf        = msg;
    current->ipc_target_pid = dest_pid;
    current->state          = TASK_BLOCKED_SEND;

    /* Cede a CPU para que outros processos executem */
    task_yield();

    return IPC_OK;
}

int32_t ipc_receive(uint32_t from_pid, message_t* msg) {
    if (!msg) return IPC_ERR;

    task_t* current = get_current_task();

    /* 1. Verifica se ja existe um remetente bloqueado tentando nos enviar */
    task_t* sender = NULL;
    if (from_pid != IPC_ANY_SENDER) {
        task_t* t = get_task_by_id(from_pid);
        if (t && t->state == TASK_BLOCKED_SEND && t->ipc_target_pid == current->id) {
            sender = t;
        }
    } else {
        /* Procura qualquer tarefa bloqueada em envio para nós */
        for (uint32_t id = 0; id < 64; id++) {
            task_t* t = get_task_by_id(id);
            if (t && t->state == TASK_BLOCKED_SEND && t->ipc_target_pid == current->id) {
                sender = t;
                break;
            }
        }
    }

    /* 2. Se ha um remetente esperando, realiza a transferencia imediata */
    if (sender && sender->ipc_buf) {
        msg->sender  = sender->ipc_buf->sender;
        msg->type    = sender->ipc_buf->type;
        msg->data[0] = sender->ipc_buf->data[0];
        msg->data[1] = sender->ipc_buf->data[1];
        msg->data[2] = sender->ipc_buf->data[2];
        msg->data[3] = sender->ipc_buf->data[3];

        /* Desbloqueia o remetente */
        sender->state = TASK_READY;
        return IPC_OK;
    }

    /* 3. Se ninguem esta enviando ainda, bloqueia aguardando mensagem */
    current->ipc_buf        = msg;
    current->ipc_target_pid = from_pid;
    current->state          = TASK_BLOCKED_RECEIVE;

    task_yield();

    return IPC_OK;
}

int32_t ipc_reply(uint32_t dest_pid, message_t* msg) {
    if (!msg) return IPC_ERR;

    task_t* current = get_current_task();
    task_t* dest    = get_task_by_id(dest_pid);

    if (!dest) return IPC_ERR;

    msg->sender = current->id;

    /* A resposta (Reply) so eh valida se o destinatario estiver aguardando */
    if (dest->state == TASK_BLOCKED_RECEIVE &&
       (dest->ipc_target_pid == current->id || dest->ipc_target_pid == IPC_ANY_SENDER)) {

        if (dest->ipc_buf) {
            dest->ipc_buf->sender  = msg->sender;
            dest->ipc_buf->type    = msg->type;
            dest->ipc_buf->data[0] = msg->data[0];
            dest->ipc_buf->data[1] = msg->data[1];
            dest->ipc_buf->data[2] = msg->data[2];
            dest->ipc_buf->data[3] = msg->data[3];
        }

        dest->state = TASK_READY;
        return IPC_OK;
    }

    return IPC_ERR;
}
