#ifndef PROCESS_H
#define PROCESS_H

/*
 * process.h - Cabeçalho do gerenciador de processos
 * Descrição: Define estruturas e protótipos para criação, gerenciamento
 *            e alternância entre processos em modo kernel e user mode.
 */

#include "kernel.h"
#include "scheduler.h"

/* ============================================================
 * CONSTANTES DE PROCESSOS
 * ============================================================ */
#define MAX_PROCESSES       32          /* Máximo de processos simultâneos */
#define MAX_THREADS_PER_PROC 8          /* Máximo de threads por processo */
#define PROC_NAME_LEN       64          /* Tamanho máximo do nome do processo */
#define USER_STACK_SIZE     (64 * 1024) /* Pilha em user mode: 64 KB */
#define USER_HEAP_BASE      0x40000000  /* Base da heap em user mode */

/* Estados de processo */
typedef enum {
    PROC_EMPTY   = 0,   /* Slot vazio */
    PROC_RUNNING = 1,   /* Processo em execução */
    PROC_WAITING = 2,   /* Processo aguardando recurso ou filho */
    PROC_ZOMBIE  = 3,   /* Processo finalizado, aguardando coleta */
} process_state_t;

/* Nível de privilégio */
typedef enum {
    PRIVILEGE_KERNEL = 0,   /* Modo kernel (ring 0) */
    PRIVILEGE_USER   = 3,   /* Modo usuário (ring 3) */
} privilege_t;

/* ============================================================
 * ESTRUTURA DE PROCESSO
 * ============================================================ */
typedef struct process {
    u32             pid;                        /* ID do processo */
    char            name[PROC_NAME_LEN];        /* Nome do processo */
    process_state_t state;                      /* Estado atual */
    privilege_t     privilege;                  /* Nível de privilégio */
    uptr            page_directory;             /* Diretório de páginas próprio */
    u32             thread_ids[MAX_THREADS_PER_PROC]; /* IDs das threads */
    u32             thread_count;               /* Número de threads */
    u32             parent_pid;                 /* PID do processo pai */
    s32             exit_code;                  /* Código de saída */
    uptr            user_stack;                 /* Endereço da pilha user */
    uptr            heap_base;                  /* Base da heap */
    u32             heap_size;                  /* Tamanho atual da heap */
    u32             entry_point;                /* Endereço de entrada */
    struct process *next;                       /* Próximo processo na lista */
} process_t;

/* ============================================================
 * PROTÓTIPOS
 * ============================================================ */

/*
 * Função: init_processes
 * Descrição: Inicializa o subsistema de gerenciamento de processos,
 *            configurando a tabela de processos e o processo inicial (idle).
 */
void init_processes(void);

/*
 * Função: create_process
 * Descrição: Cria um novo processo com o nome e ponto de entrada especificados.
 *            Aloca espaço de endereçamento próprio e configura as estruturas
 *            necessárias. Retorna o PID ou -1 em caso de erro.
 */
int create_process(const char *name, u32 entry_point, privilege_t privilege);

/*
 * Função: terminate_process
 * Descrição: Finaliza o processo com o PID especificado, liberando todos
 *            os recursos alocados incluindo memória, threads e arquivos abertos.
 */
void terminate_process(u32 pid, s32 exit_code);

/*
 * Função: get_current_process
 * Descrição: Retorna o ponteiro para o processo atualmente em execução.
 */
process_t *get_current_process(void);

/*
 * Função: get_process_by_pid
 * Descrição: Busca e retorna o processo com o PID especificado.
 *            Retorna NULL se não encontrado.
 */
process_t *get_process_by_pid(u32 pid);

/*
 * Função: switch_to_user_mode
 * Descrição: Realiza a transição do modo kernel (ring 0) para o modo
 *            usuário (ring 3), configurando os segmentos e a pilha corretamente.
 *            Usa instrução IRET para a transição segura.
 */
void switch_to_user_mode(u32 user_esp, u32 user_eip);

/*
 * Função: list_processes
 * Descrição: Exibe no terminal a lista de todos os processos ativos
 *            com seus PIDs, nomes, estados e uso de recursos.
 */
void list_processes(void);

/*
 * Função: exec_program
 * Descrição: Carrega e executa um programa ELF a partir do caminho
 *            especificado, criando um novo processo para ele.
 */
int exec_program(const char *path);

#endif /* PROCESS_H */
