#ifndef SCHEDULER_H
#define SCHEDULER_H

/*
 * scheduler.h - Cabeçalho do escalonador de threads
 * Descrição: Define estruturas e protótipos para gerenciamento de
 *            threads e escalonamento de processos do kernel.
 */

#include "kernel.h"

/* ============================================================
 * CONSTANTES DO SCHEDULER
 * ============================================================ */
#define MAX_THREADS         64          /* Máximo de threads simultâneas */
#define THREAD_STACK_SIZE   8192        /* Tamanho da pilha por thread (8 KB) */
#define QUANTUM_MS          10          /* Quantum de tempo por thread (ms) */

/* Estados possíveis de uma thread */
typedef enum {
    THREAD_EMPTY    = 0,    /* Slot vazio - sem thread */
    THREAD_READY    = 1,    /* Thread pronta para execução */
    THREAD_RUNNING  = 2,    /* Thread em execução atual */
    THREAD_BLOCKED  = 3,    /* Thread bloqueada aguardando recurso */
    THREAD_SLEEPING = 4,    /* Thread em modo de espera temporária */
    THREAD_DEAD     = 5,    /* Thread finalizada, aguardando limpeza */
} thread_state_t;

/* ============================================================
 * CONTEXTO DE CPU (registradores salvos)
 * ============================================================ */
typedef struct {
    u64 rdi, rsi, rbp, rsp;
    u64 rbx, rdx, rcx, rax;
    u64 rip;
    u64 rflags;
} PACKED cpu_context_t;

/* ============================================================
 * ESTRUTURA DE THREAD
 * ============================================================ */
typedef struct thread {
    u32             id;             /* Identificador único da thread */
    char            name[32];       /* Nome descritivo da thread */
    thread_state_t  state;          /* Estado atual da thread */
    cpu_context_t   context;        /* Contexto salvo da CPU */
    u8             *stack;          /* Ponteiro para a pilha da thread */
    u32             stack_size;     /* Tamanho da pilha */
    u32             priority;       /* Prioridade (1=baixa, 10=alta) */
    u32             ticks;          /* Ticks de CPU consumidos */
    u32             sleep_ticks;    /* Ticks restantes para acordar */
    u32             process_id;     /* ID do processo pai */
    struct thread  *next;           /* Próxima thread na fila */
} thread_t;

/* ============================================================
 * PROTÓTIPOS DO SCHEDULER
 * ============================================================ */

/*
 * Função: init_scheduler
 * Descrição: Inicializa o escalonador de threads configurando as
 *            estruturas internas e registrando o timer de interrupção.
 */
void init_scheduler(void);

/*
 * Função: create_thread
 * Descrição: Cria uma nova thread com a função de entrada e prioridade
 *            especificadas. Retorna o ID da thread ou -1 em caso de erro.
 */
int create_thread(const char *name, void (*entry)(void), u32 priority);

/*
 * Função: schedule
 * Descrição: Executa o algoritmo de escalonamento e seleciona a próxima
 *            thread a ser executada (round-robin com prioridade).
 */
void schedule(void);

/*
 * Função: switch_context
 * Descrição: Realiza a troca de contexto entre a thread atual e a próxima,
 *            salvando e restaurando os registradores da CPU.
 */
void switch_context(cpu_context_t *old_ctx, cpu_context_t *new_ctx);

/*
 * Função: terminate_thread
 * Descrição: Finaliza a thread com o ID especificado, liberando
 *            seus recursos (pilha e estruturas de dados).
 */
void terminate_thread(u32 thread_id);

/*
 * Função: get_current_thread
 * Descrição: Retorna o ponteiro para a thread atualmente em execução.
 */
thread_t *get_current_thread(void);

/*
 * Função: thread_sleep
 * Descrição: Coloca a thread atual em estado de espera pelo número
 *            de milissegundos especificado.
 */
void thread_sleep(u32 ms);

/*
 * Função: list_threads
 * Descrição: Exibe no terminal a lista de todas as threads ativas
 *            com seus estados e informações de uso.
 */
void list_threads(void);

/*
 * Função: get_thread_count
 * Descrição: Retorna o número de threads atualmente ativas no sistema.
 */
u32 get_thread_count(void);

#endif /* SCHEDULER_H */
