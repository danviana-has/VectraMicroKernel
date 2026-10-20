#ifndef VECTRA_TASK_H
#define VECTRA_TASK_H

#include "types.h"
#include "ipc.h"

typedef enum {
    TASK_READY = 0,
    TASK_RUNNING,
    TASK_BLOCKED_SEND,
    TASK_BLOCKED_RECEIVE,
    TASK_SLEEPING,
    TASK_DEAD
} task_state_t;

typedef struct task {
    uint32_t     id;                  /* PID */
    char         name[32];            /* Nome da tarefa */
    uint32_t     esp;                 /* Ponteiro de pilha salvo */
    uint32_t     stack_base;          /* Endereco base alocado para a pilha */
    uint32_t     kernel_stack;        /* Topo da pilha do kernel para TSS */
    uint32_t     page_directory;      /* CR3 da tarefa */
    task_state_t state;               /* Estado de execucao */
    uint32_t     sleep_ticks;         /* Contador de sono */
    
    /* Suporte a processos de Usuario (Ring 3) */
    bool         is_user;             /* Verdadeiro se a tarefa roda em Ring 3 */
    uint32_t     user_entry;          /* Endereco virtual de entrada em Ring 3 */
    uint32_t     user_stack;          /* Topo da pilha de usuario em Ring 3 */

    /* Suporte a IPC sincronizado */
    message_t*   ipc_buf;             /* Ponteiro para o buffer de mensagem */
    uint32_t     ipc_target_pid;      /* Destinatario ou remetente esperado */

    struct task* next;                /* Proxima tarefa na fila circular */
} task_t;

void    task_init(void);
task_t* task_create(const char* name, void (*entry_point)(void));
task_t* task_create_user(const char* name, uint32_t entry_point, uint32_t user_stack, uint32_t page_directory);
void    schedule(void);
void    task_yield(void);
void    task_exit(void);
task_t* get_current_task(void);
task_t* get_task_by_id(uint32_t id);
void    task_sleep(uint32_t ticks);
void    task_destroy(task_t* task);

#endif /* VECTRA_TASK_H */
