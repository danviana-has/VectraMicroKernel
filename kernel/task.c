#include "task.h"
#include "heap.h"
#include "tss.h"
#include "drivers.h"

extern void task_switch_asm(uint32_t* old_esp, uint32_t new_esp, uint32_t new_cr3, uint32_t kernel_stack);
extern void user_jump_trampoline(void);

static task_t* current_task    = NULL;
static task_t* task_list_head  = NULL;
static uint32_t next_task_id   = 1;
static bool    scheduler_ready = false;

task_t* get_current_task(void) {
    return current_task;
}

task_t* get_task_by_id(uint32_t id) {
    task_t* t = task_list_head;
    if (!t) return NULL;
    do {
        if (t->id == id && t->state != TASK_DEAD) {
            return t;
        }
        t = t->next;
    } while (t != task_list_head);
    return NULL;
}

void task_init(void) {
    /* 1. Cria a estrutura da tarefa principal atual (kmain - PID 0) */
    task_t* main_task = (task_t*)kmalloc(sizeof(task_t));
    main_task->id             = 0;
    main_task->state          = TASK_RUNNING;
    main_task->sleep_ticks    = 0;
    main_task->ipc_buf        = NULL;
    main_task->ipc_target_pid = 0;
    main_task->is_user        = false;
    main_task->user_entry     = 0;
    main_task->user_stack     = 0;
    main_task->stack_base     = 0;
    main_task->kernel_stack   = 0x00100000; /* Pilha inicial do boot */

    uint32_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    main_task->page_directory = cr3;

    /* Copia o nome */
    const char* k_name = "kernel_main";
    int i = 0;
    while (k_name[i] && i < 31) {
        main_task->name[i] = k_name[i];
        i++;
    }
    main_task->name[i] = '\0';

    main_task->next = main_task; /* Lista circular unitaria */
    task_list_head  = main_task;
    current_task    = main_task;

    /* 2. Inicializa o Task State Segment (TSS) para transicoes de privilegio */
    tss_init(main_task->kernel_stack);

    scheduler_ready = true;
}

task_t* task_create(const char* name, void (*entry_point)(void)) {
    task_t* t = (task_t*)kmalloc(sizeof(task_t));
    if (!t) return NULL;

    t->id             = next_task_id++;
    t->state          = TASK_READY;
    t->sleep_ticks    = 0;
    t->ipc_buf        = NULL;
    t->ipc_target_pid = 0;
    t->is_user        = false;
    t->user_entry     = 0;
    t->user_stack     = 0;

    int i = 0;
    while (name[i] && i < 31) {
        t->name[i] = name[i];
        i++;
    }
    t->name[i] = '\0';

    /* Aloca 16 KiB de pilha para a nova tarefa */
    uint32_t stack_mem = (uint32_t)kmalloc(16384);
    t->stack_base   = stack_mem;
    t->kernel_stack = stack_mem + 16384;

    /* Compartilha o espaco virtual inicial do kernel */
    uint32_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    t->page_directory = cr3;

    /*
     * Monta o frame inicial na pilha para o primeiro retorno via task_switch_asm:
     * O topo contera:
     *   [ret addr = entry_point]
     *   [pushfd = 0x0202 (IF=1)]
     *   [ebp = 0]
     *   [edi = 0]
     *   [esi = 0]
     *   [ebx = 0]
     */
    uint32_t* stack = (uint32_t*)(t->kernel_stack);

    *(--stack) = (uint32_t)entry_point; /* Endereco de retorno da instrucao ret */
    *(--stack) = 0x0202;                /* EFLAGS inicial com interrupcoes habilitadas (IF=1) */
    *(--stack) = 0;                     /* EBP */
    *(--stack) = 0;                     /* EDI */
    *(--stack) = 0;                     /* ESI */
    *(--stack) = 0;                     /* EBX */

    t->esp = (uint32_t)stack;

    /* Insere a nova tarefa na lista circular */
    t->next = task_list_head->next;
    task_list_head->next = t;

    return t;
}

task_t* task_create_user(const char* name, uint32_t entry_point, uint32_t user_stack, uint32_t page_directory) {
    task_t* t = (task_t*)kmalloc(sizeof(task_t));
    if (!t) return NULL;

    t->id             = next_task_id++;
    t->state          = TASK_READY;
    t->sleep_ticks    = 0;
    t->ipc_buf        = NULL;
    t->ipc_target_pid = 0;
    t->is_user        = true;
    t->user_entry     = entry_point;
    t->user_stack     = user_stack;

    int i = 0;
    while (name[i] && i < 31) {
        t->name[i] = name[i];
        i++;
    }
    t->name[i] = '\0';

    /* Aloca 16 KiB de pilha do KERNEL para a tarefa de usuario (usada pelo TSS e ISRs) */
    uint32_t stack_mem = (uint32_t)kmalloc(16384);
    t->stack_base   = stack_mem;
    t->kernel_stack = stack_mem + 16384;

    /* Define o espaco de pagina isolado da tarefa */
    if (page_directory != 0) {
        t->page_directory = page_directory;
    } else {
        uint32_t cr3;
        __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
        t->page_directory = cr3;
    }

    /*
     * Monta o frame inicial na pilha do kernel:
     * 1. Frame de retorno da instrucao iret para o Ring 3 (user_jump_trampoline):
     *    [SS = 0x23 (User Data)]
     *    [ESP = user_stack]
     *    [EFLAGS = 0x0202 (IF=1)]
     *    [CS = 0x1B (User Code)]
     *    [EIP = entry_point]
     *
     * 2. Frame de retorno da rotina task_switch_asm:
     *    [ret = user_jump_trampoline]
     *    [pushfd = 0x0202]
     *    [ebp = 0]
     *    [edi = 0]
     *    [esi = 0]
     *    [ebx = 0]
     */
    uint32_t* stack = (uint32_t*)(t->kernel_stack);

    *(--stack) = 0x23;                      /* SS (User Data RPL 3) */
    *(--stack) = user_stack;                /* ESP de Usuario */
    *(--stack) = 0x0202;                    /* EFLAGS com IF=1 */
    *(--stack) = 0x1B;                      /* CS (User Code RPL 3) */
    *(--stack) = entry_point;               /* EIP de Usuario */

    *(--stack) = (uint32_t)user_jump_trampoline;
    *(--stack) = 0x0202;                    /* EFLAGS para popfd */
    *(--stack) = 0;                         /* EBP */
    *(--stack) = 0;                         /* EDI */
    *(--stack) = 0;                         /* ESI */
    *(--stack) = 0;                         /* EBX */

    t->esp = (uint32_t)stack;

    /* Insere a nova tarefa na lista circular */
    t->next = task_list_head->next;
    task_list_head->next = t;

    return t;
}

void schedule(void) {
    if (!scheduler_ready || !current_task) return;

    uint32_t eflags;
    __asm__ volatile ("pushfl; popl %0; cli" : "=r"(eflags));

    /* 1. Atualiza tarefas adormecidas */
    task_t* iter = task_list_head;
    do {
        if (iter->state == TASK_SLEEPING && iter->sleep_ticks > 0) {
            iter->sleep_ticks--;
            if (iter->sleep_ticks == 0) {
                iter->state = TASK_READY;
            }
        }
        iter = iter->next;
    } while (iter != task_list_head);

    /* 2. Seleciona a proxima tarefa pronta em Round-Robin */
    task_t* next_task = current_task->next;
    while (next_task != current_task) {
        if (next_task->state == TASK_READY || next_task->state == TASK_RUNNING) {
            break;
        }
        next_task = next_task->next;
    }

    if (next_task == current_task) {
        if (eflags & 0x0200) {
            __asm__ volatile ("sti");
        }
        return; /* Nenhuma outra tarefa disponivel para alternar */
    }

    /* 3. Troca de Contexto preemptiva */
    task_t* old_task = current_task;
    if (old_task->state == TASK_RUNNING) {
        old_task->state = TASK_READY;
    }
    next_task->state = TASK_RUNNING;
    current_task = next_task;

    task_switch_asm(&old_task->esp, next_task->esp, next_task->page_directory, next_task->kernel_stack);

    if (eflags & 0x0200) {
        __asm__ volatile ("sti");
    }
}

void task_destroy(task_t* task) {
    if (!task || task == task_list_head) return;

    uint32_t eflags;
    __asm__ volatile ("pushfl; popl %0; cli" : "=r"(eflags));

    task_t* prev = task_list_head;
    while (prev->next != task && prev->next != task_list_head) {
        prev = prev->next;
    }
    if (prev->next == task) {
        prev->next = task->next;
    }

    if (task->stack_base) {
        kfree((void*)task->stack_base);
    }
    kfree(task);

    if (eflags & 0x0200) {
        __asm__ volatile ("sti");
    }
}

extern void net_poll(void);

void task_yield(void) {
    net_poll();
    __asm__ volatile ("cli");
    schedule();
    __asm__ volatile ("sti");
}

void task_sleep(uint32_t ticks) {
    __asm__ volatile ("cli");
    current_task->sleep_ticks = ticks;
    current_task->state = TASK_SLEEPING;
    schedule();
    __asm__ volatile ("sti");
}

void task_exit(void) {
    __asm__ volatile ("cli");
    current_task->state = TASK_DEAD;
    schedule();
    for (;;) __asm__ volatile ("hlt");
}
