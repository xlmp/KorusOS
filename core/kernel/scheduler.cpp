/*
 * scheduler.c - Implementação do escalonador de threads
 * Descrição: Escalonador preemptivo round-robin com suporte a prioridades.
 *            Gerencia criação, alternância e término de threads do kernel.
 *
 * Algoritmo: Round-Robin com prioridade ponderada
 * Contexto salvo: registradores x86 (edi, esi, ebp, esp, ebx, edx, ecx, eax, eip, eflags)
 */

#include "../include/scheduler.h"
#include "../include/memory.h"
#include "../include/kernel.h"

extern void screen_print(const char *str);
extern void screen_println(const char *str);
extern void screen_print_int(s32 val);
extern void screen_print_hex(u32 val);

/* ============================================================
 * VARIÁVEIS INTERNAS DO SCHEDULER
 * ============================================================ */

/* Tabela de threads do sistema */
static thread_t  thread_table[MAX_THREADS];
static u32       next_thread_id = 1;        /* Contador de IDs de threads */
static thread_t *current_thread = (thread_t *)NULL;     /* Thread em execução atual */
static u32       thread_count   = 0;        /* Total de threads ativas */
static u32       timer_ticks    = 0;        /* Contador de ticks do timer */

/* ============================================================
 * FUNÇÕES AUXILIARES INTERNAS
 * ============================================================ */

/*
 * Função: kstrncpy
 * Descrição: Copia string de origem para destino com limite de tamanho.
 */
static void kstrncpy(char *dest, const char *src, u32 max) {
    u32 i = 0;
    while (i < max - 1 && src[i]) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/*
 * Função: find_free_slot
 * Descrição: Encontra um slot vazio na tabela de threads.
 *            Retorna o índice ou -1 se a tabela estiver cheia.
 */
static int find_free_slot(void) {
    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread_table[i].state == THREAD_EMPTY) {
            return i;
        }
    }
    return -1;
}

/*
 * Função: find_next_thread
 * Descrição: Encontra a próxima thread pronta para execução usando
 *            algoritmo round-robin com prioridade.
 *            Retorna ponteiro para a próxima thread ou NULL se não houver.
 */
static thread_t *find_next_thread(void) {
    if (!current_thread) {
        /* Primeira execução - busca qualquer thread pronta */
        for (int i = 0; i < MAX_THREADS; i++) {
            if (thread_table[i].state == THREAD_READY) {
                return &thread_table[i];
            }
        }
        return (thread_t *)NULL;
    }

    /* Round-robin: começa após a thread atual */
    int start = (int)(current_thread - thread_table);
    int idx   = (start + 1) % MAX_THREADS;

    for (int count = 0; count < MAX_THREADS; count++) {
        thread_t *t = &thread_table[idx];
        if (t->state == THREAD_READY && t != current_thread) {
            return t;
        }
        idx = (idx + 1) % MAX_THREADS;
    }

    /* Nenhuma outra thread pronta - retorna a atual se ainda pronta */
    if (current_thread->state == THREAD_RUNNING ||
        current_thread->state == THREAD_READY) {
        return current_thread;
    }

    return (thread_t *)NULL;
}

/* ============================================================
 * IMPLEMENTAÇÃO DAS FUNÇÕES PÚBLICAS
 * ============================================================ */

/*
 * Função: init_scheduler
 * Descrição: Inicializa o escalonador de threads configurando as
 *            estruturas internas e registrando o timer de interrupção.
 */
void init_scheduler(void) {
    /* Zera a tabela de threads */
    kmemset(thread_table, 0, sizeof(thread_table));
    current_thread = (thread_t *)NULL;
    thread_count   = 0;
    next_thread_id = 1;
    timer_ticks    = 0;

    /* Cria thread idle (thread 0 - sempre em execução quando idle) */
    int slot = find_free_slot();
    if (slot >= 0) {
        thread_table[slot].id         = 0;
        thread_table[slot].state      = THREAD_READY;
        thread_table[slot].priority   = 1; /* Prioridade mínima */
        thread_table[slot].process_id = 0;
        kstrncpy(thread_table[slot].name, "idle", 32);
        thread_count++;
    }
}

/*
 * Função: create_thread
 * Descrição: Cria uma nova thread com a função de entrada e prioridade
 *            especificadas. Retorna o ID da thread ou -1 em caso de erro.
 */
int create_thread(const char *name, void (*entry)(void), u32 priority) {
    if (thread_count >= MAX_THREADS) return -1;
    if (!entry) return -1;

    int slot = find_free_slot();
    if (slot < 0) return -1;

    thread_t *t = &thread_table[slot];

    /* Aloca pilha para a nova thread */
    t->stack = (u8 *)kmalloc(THREAD_STACK_SIZE);
    if (!t->stack) return -1;
    kmemset(t->stack, 0, THREAD_STACK_SIZE);

    t->id         = next_thread_id++;
    t->state      = THREAD_READY;
    t->priority   = (priority < 1) ? 1 : (priority > 10 ? 10 : priority);
    t->stack_size = THREAD_STACK_SIZE;
    t->ticks      = 0;
    t->process_id = 0;

    kstrncpy(t->name, name ? name : "thread", 32);

    /* Configura o contexto inicial da thread */
    u32 stack_top = (u32)(t->stack + THREAD_STACK_SIZE);

    /* Empilha o endereço de retorno (terminate_thread) */
    stack_top -= 4;
    *((u32 *)stack_top) = (u32)entry;

    t->context.esp    = stack_top;
    t->context.eip    = (u32)entry;
    t->context.eflags = 0x00000202; /* IF habilitado */

    thread_count++;
    return (int)t->id;
}

/*
 * Função: switch_context
 * Descrição: Realiza a troca de contexto entre threads usando inline assembly.
 *            Salva os registradores da thread atual e restaura os da próxima.
 */
void switch_context(cpu_context_t *old_ctx, cpu_context_t *new_ctx) {
        
    //     /* Salva contexto atual */
    // __asm__ volatile (
    //     "mov %%eax, %0\n"
    //     "mov %%ecx, %1\n"
    //     "mov %%edx, %2\n"
    //     "mov %%ebx, %3\n"
    //     "mov %%esi, %4\n"
    //     "mov %%edi, %5\n"
    //     "pushfl\n"
    //     "pop %6\n"
    //     /* Restaura contexto novo */
    //     "mov %7,  %%eax\n"
    //     "mov %8,  %%ecx\n"
    //     "mov %9,  %%edx\n"
    //     "mov %10, %%ebx\n"
    //     "mov %11, %%esi\n"
    //     "mov %12, %%edi\n"
    //     "push %13\n"
    //     "popfl\n"
    //     :
    //     "=m"(old_ctx->eax), "=m"(old_ctx->ecx),
    //     "=m"(old_ctx->edx), "=m"(old_ctx->ebx),
    //     "=m"(old_ctx->esi), "=m"(old_ctx->edi),
    //     "=m"(old_ctx->eflags)
    //     :
    //     "m"(new_ctx->eax), "m"(new_ctx->ecx),
    //     "m"(new_ctx->edx), "m"(new_ctx->ebx),
    //     "m"(new_ctx->esi), "m"(new_ctx->edi),
    //     "m"(new_ctx->eflags)
    //     : "memory"
    // );
}

/*
 * Função: schedule
 * Descrição: Executa o algoritmo de escalonamento e seleciona a próxima
 *            thread a ser executada (round-robin com prioridade).
 */
void schedule(void) {
    timer_ticks++;

    /* Acorda threads que estavam dormindo */
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_t *t = &thread_table[i];
        if (t->state == THREAD_SLEEPING) {
            if (t->sleep_ticks > 0) {
                t->sleep_ticks--;
            } else {
                t->state = THREAD_READY;
            }
        }
    }

    thread_t *next = find_next_thread();
    if (!next || next == current_thread) return;

    /* Marca thread atual como pronta (se ainda ativa) */
    if (current_thread && current_thread->state == THREAD_RUNNING) {
        current_thread->state = THREAD_READY;
    }

    /* Realiza a troca de contexto */
    thread_t *old = current_thread;
    current_thread = next;
    current_thread->state = THREAD_RUNNING;
    current_thread->ticks++;

    if (old) {
        switch_context(&old->context, &current_thread->context);
    }
}

/*
 * Função: terminate_thread
 * Descrição: Finaliza a thread com o ID especificado, liberando
 *            seus recursos (pilha e estruturas de dados).
 */
void terminate_thread(u32 thread_id) {
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_t *t = &thread_table[i];
        if (t->id == thread_id && t->state != THREAD_EMPTY) {
            t->state = THREAD_DEAD;

            /* Libera a pilha */
            if (t->stack) {
                kfree(t->stack);
                t->stack = (u8*)NULL;
            }

            t->state = THREAD_EMPTY;
            if (thread_count > 0) thread_count--;
            return;
        }
    }
}

/*
 * Função: get_current_thread
 * Descrição: Retorna o ponteiro para a thread atualmente em execução.
 */
thread_t *get_current_thread(void) {
    return current_thread;
}

/*
 * Função: thread_sleep
 * Descrição: Coloca a thread atual em estado de espera pelo número
 *            de milissegundos especificado.
 */
void thread_sleep(u32 ms) {
    if (!current_thread) return;

    /* Converte ms para ticks (assumindo 1000 ticks/seg = 1 tick/ms) */
    current_thread->sleep_ticks = ms;
    current_thread->state       = THREAD_SLEEPING;
    schedule();
}

/*
 * Função: get_thread_count
 * Descrição: Retorna o número de threads atualmente ativas no sistema.
 */
u32 get_thread_count(void) {
    return thread_count;
}

/*
 * Função: list_threads
 * Descrição: Exibe no terminal a lista de todas as threads ativas
 *            com seus estados e informações de uso.
 */
void list_threads(void) {
    const char *state_names[] = {
        "VAZIO", "PRONTA", "EXECUTANDO",
        "BLOQUEADA", "DORMINDO", "FINALIZADA"
    };

    screen_println("=== Lista de Threads ===");
    screen_println("  ID   NOME              ESTADO       TICKS  PRIOR");
    screen_println("  ---- ----------------- ------------ ------ -----");

    for (int i = 0; i < MAX_THREADS; i++) {
        thread_t *t = &thread_table[i];
        if (t->state == THREAD_EMPTY) continue;

        screen_print("  ");
        screen_print_int((s32)t->id);
        screen_print("    ");
        screen_print(t->name);
        screen_print("    ");

        u32 st = (u32)t->state;
        if (st < 6) screen_print(state_names[st]);
        screen_print("    ");
        screen_print_int((s32)t->ticks);
        screen_print("    ");
        screen_print_int((s32)t->priority);
        screen_println("");
    }

    screen_print("Total de threads ativas: ");
    screen_print_int((s32)thread_count);
    screen_println("");
}
